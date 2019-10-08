/*
 * pipe.c -- fifo driver for scull
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 * Modified for warning-free compilation under Linux 2.6.31 by baker@cs.fsu.edu.
 * Modified for execution under 3.5 kernels by franco@gauss.ececs.uc.edu
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>        /* TASK_INTERRUPTIBLE */ //tpb
/* Dependence on sched.h indicates the code probably is obsolescent.
   Should not be directly manipulating task state. */
#include <linux/kernel.h>       /* printk(), min() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/proc_fs.h>
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "scull.h"              /* local definitions */

struct scull_pipe {
   wait_queue_head_t inq, outq;       /* read and write queues */
   char *buffer, *end;                /* begin of buf, end of buf */
   int buffersize;                    /* used in pointer arithmetic */
   char *rp, *wp;                     /* where to read, where to write */
   int nreaders, nwriters;            /* number of openings for r/w */
   struct fasync_struct *async_queue; /* asynchronous readers */
   struct semaphore sem;              /* mutual exclusion semaphore */
   struct cdev cdev;                  /* Char device structure */
};

/* parameters */
static int scull_p_nr_devs = SCULL_P_NR_DEVS;   /* number of pipe devices */
int scull_p_buffer =  SCULL_P_BUFFER;   /* buffer size */
dev_t scull_p_devno;                    /* Our first device number */

module_param(scull_p_nr_devs, int, 0);  /* FIXME check perms */
module_param(scull_p_buffer, int, 0);

static struct scull_pipe *scull_p_devices;

static int scull_p_fasync(int fd, struct file *filp, int mode);
static int spacefree(struct scull_pipe *dev);


static int scull_p_open(struct inode *inode, struct file *filp) {
   struct scull_pipe *dev;
   
   dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
   filp->private_data = dev;
   
   if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
   if (!dev->buffer) {
      /* allocate the buffer */
      dev->buffer = kmalloc(scull_p_buffer, GFP_KERNEL);
      if (!dev->buffer) {
	 up(&dev->sem);
	 return -ENOMEM;
      }
   }
   dev->buffersize = scull_p_buffer;
   dev->end = dev->buffer + dev->buffersize;
   dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */
   
   /* use f_mode, not f_flags: it's cleaner (fs/open.c tells why) */
   if (filp->f_mode & FMODE_READ) dev->nreaders++;
   if (filp->f_mode & FMODE_WRITE) dev->nwriters++;
   up(&dev->sem);
   
   return nonseekable_open(inode, filp);
}

static int scull_p_release(struct inode *inode, struct file *filp) {
   struct scull_pipe *dev = filp->private_data;
   
   /* remove this filp from the asynchronously notified filp's */
   scull_p_fasync(-1, filp, 0);
   down(&dev->sem);
   if (filp->f_mode & FMODE_READ)
      dev->nreaders--;
   if (filp->f_mode & FMODE_WRITE)
      dev->nwriters--;
   if (dev->nreaders + dev->nwriters == 0) {
      kfree(dev->buffer);
      dev->buffer = NULL; /* the other fields are not checked on open */
   }
   up(&dev->sem);
   return 0;
}

static ssize_t scull_p_read (struct file *filp, 
			     char __user *buf, 
			     size_t count,
			     loff_t *f_pos) {

   struct scull_pipe *dev = filp->private_data;
   
   if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
   
   /*
     The while loop tests the buffer with the device semaphore held. If
     there is data there, we know we can return it to the user immediately
     without sleeping, so the entire body of the loop is skipped.
   */
   while (dev->rp == dev->wp) { /* nothing to read */
      up(&dev->sem); /* release the lock */

      /* return if the user has requested non-blocking I/O */
      if (filp->f_flags & O_NONBLOCK) return -EAGAIN;

      /* otherwise go to sleep */
      PDEBUG("\"%s\" reading: going to sleep\n", current->comm);

      /*
	Something has awakened us but we do not know what.  One
	possibility is that the process received a signal.  The if
	statement that contains the wait_event_interruptible call
	checks for this case. This statement ensures the proper and
	expected reaction to signals, which could have been
	responsible for waking up the process.  If a signal has
	arrived and it has not been blocked by the process, the
	proper behavior is to let upper layers of the kernel handle
	the event.  To this end, the driver returns -ERESTARTSYS to
	the caller; this value is used internally by the virtual
	filesystem (VFS) layer, which either restarts the system call
	or returns -EINTR to user space.
      */
      if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
	 return -ERESTARTSYS; /* signal: tell the fs layer to handle it */

      /*
         However, even in the absence of a signal, we do not yet know
         for sure that there is data there for the taking.  Somebody
         else could have been waiting for data as well, and they might
         win the race and get the data first. So we must acquire the
         device semaphore again; only then can we test the read buffer
         again (in the while loop) and truly know that we can return
         the data in the buffer to the user.
      */
      if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
   }

   /*
      We know that the semaphore is held and the buffer contains data
      that we can use.  We can now read the data
   */
   if (dev->wp > dev->rp) count = min(count, (size_t)(dev->wp - dev->rp));
   else /* the write pointer has wrapped, return data up to dev->end */
      count = min(count, (size_t)(dev->end - dev->rp));

   if (copy_to_user(buf, dev->rp, count)) {
      up (&dev->sem);
      return -EFAULT;
   }

   dev->rp += count;
   if (dev->rp == dev->end) dev->rp = dev->buffer; /* wrapped */
   up (&dev->sem);
   
   /* finally, awaken any writers and return */
   wake_up_interruptible(&dev->outq);
   PDEBUG("\"%s\" did read %li bytes\n", current->comm, (long)count);
   return count;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_pipe *dev, struct file *filp) {
   while (spacefree(dev) == 0) { /* full */
      DEFINE_WAIT(wait);
      
      up(&dev->sem);
      if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
      PDEBUG("\"%s\" writing: going to sleep\n",current->comm);

      prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
      if (spacefree(dev) == 0) schedule();

      finish_wait(&dev->outq, &wait);

      /* signal: tell the fs layer to handle it */
      if (signal_pending(current)) return -ERESTARTSYS;
      if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
   }
   return 0;
}       

/* How much space is free? */
static int spacefree(struct scull_pipe *dev) {
   if (dev->rp == dev->wp) return dev->buffersize - 1;
   return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

ssize_t scull_p_write(struct file *filp, 
		      const char __user *buf, 
		      size_t count,
		      loff_t *f_pos) {
   struct scull_pipe *dev = filp->private_data;
   int result;
   
   if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
   
   /* Make sure there's space to write */
   result = scull_getwritespace(dev, filp);
   if (result) return result; /* scull_getwritespace called up(&dev->sem) */
   
   /* ok, space is there, accept something */
   count = min(count, (size_t)spacefree(dev));
   if (dev->wp >= dev->rp)
      count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
   else /* the write pointer has wrapped, fill up to rp-1 */
      count = min(count, (size_t)(dev->rp - dev->wp - 1));
   PDEBUG("Accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
   if (copy_from_user(dev->wp, buf, count)) {
      up (&dev->sem);
      return -EFAULT;
   }
   dev->wp += count;
   if (dev->wp == dev->end) dev->wp = dev->buffer; /* wrapped */
   up(&dev->sem);
   
   /* finally, awake any reader */
   wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */
   
   /* and signal asynchronous readers, explained late in chapter 5 */
   if (dev->async_queue)
      kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
   PDEBUG("\"%s\" did write %li bytes\n",current->comm, (long)count);
   return count;
}

static unsigned int scull_p_poll(struct file *filp, poll_table *wait) {
   struct scull_pipe *dev = filp->private_data;
   unsigned int mask = 0;
   
   /*
    * The buffer is circular; it is considered full
    * if "wp" is right behind "rp" and empty if the
    * two are equal.
    */
   down(&dev->sem);
   poll_wait(filp, &dev->inq,  wait);
   poll_wait(filp, &dev->outq, wait);
   if (dev->rp != dev->wp) mask |= POLLIN | POLLRDNORM;    /* readable */
   if (spacefree(dev)) mask |= POLLOUT | POLLWRNORM;   /* writable */
   up(&dev->sem);
   return mask;
}

static int scull_p_fasync(int fd, struct file *filp, int mode) {
   struct scull_pipe *dev = filp->private_data;
   return fasync_helper(fd, filp, mode, &dev->async_queue);
}

/* FIXME this should use seq_file */
#ifdef SCULL_DEBUG
static void scullp_proc_offset(char *buf, char **start, off_t *offset, int *ln){
   if (*offset == 0)
      return;
   if (*offset >= *ln) {  /* Not there yet */
      *offset -= *ln;
      *ln = 0;
   }
   else {                  /* We're into the interesting stuff now */
      *start = buf + *offset;
      *offset = 0;
   }
}

static int scull_read_p_mem(char *buf, 
			    char **start, 
			    off_t offset, 
			    int count,
			    int *eof, 
			    void *data) {
   int i, len;
   struct scull_pipe *p;
   
#define LIMIT (PAGE_SIZE-200)   /* don't print any more after this size */
   *start = buf;
   len = sprintf(buf, "Default buffersize is %i\n", scull_p_buffer);
   for (i = 0; i < scull_p_nr_devs && len <= LIMIT; i++) {
      p = &scull_p_devices[i];
      if (down_interruptible(&p->sem)) return -ERESTARTSYS;
      len += sprintf(buf+len, "\nDevice %i: %p\n", i, p);
      /* len += sprintf(buf+len, "   Queues: %p %p\n", p->inq, p->outq);*/
      len += sprintf(buf+len, "   Buffer: %p to %p (%i bytes)\n", 
		     p->buffer, p->end, p->buffersize);
      len += sprintf(buf+len, "   rp %p   wp %p\n", p->rp, p->wp);
      len += sprintf(buf+len, "   readers %i   writers %i\n", 
		     p->nreaders, p->nwriters);
      up(&p->sem);
      scullp_proc_offset(buf, start, &offset, &len);
   }
   *eof = (len <= LIMIT);
   return len;
}
#endif

/*
 * The file operations for the pipe device
 * (some are overlayed with bare scull)
 */
struct file_operations scull_pipe_fops = {
   .owner =        THIS_MODULE,
   .llseek =       no_llseek,
   .read =         scull_p_read,
   .write =        scull_p_write,
   .poll =         scull_p_poll,
   .unlocked_ioctl =        scull_ioctl,
   .open =         scull_p_open,
   .release =      scull_p_release,
   .fasync =       scull_p_fasync,
};

/*
 * Set up a cdev entry.
 */
static void scull_p_setup_cdev(struct scull_pipe *dev, int index) {
   int err, devno = scull_p_devno + index;
   
   cdev_init(&dev->cdev, &scull_pipe_fops);
   dev->cdev.owner = THIS_MODULE;
   err = cdev_add (&dev->cdev, devno, 1);
   /* Fail gracefully if need be */
   if (err) printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

/*
 * Initialize the pipe devs; return how many we did.
 */
int scull_p_init(dev_t firstdev) {
   int i, result;
   
   result = register_chrdev_region(firstdev, scull_p_nr_devs, "scullp");
   if (result < 0) {
      printk(KERN_NOTICE "Unable to get scullp region, error %d\n", result);
      return 0;
   }
   scull_p_devno = firstdev;
   scull_p_devices = 
      kmalloc(scull_p_nr_devs*sizeof(struct scull_pipe), GFP_KERNEL);
   if (scull_p_devices == NULL) {
      unregister_chrdev_region(firstdev, scull_p_nr_devs);
      return 0;
   }
   memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(struct scull_pipe));
   for (i = 0; i < scull_p_nr_devs; i++) {
      init_waitqueue_head(&(scull_p_devices[i].inq));
      init_waitqueue_head(&(scull_p_devices[i].outq));
      sema_init(&scull_p_devices[i].sem, 1);
      scull_p_setup_cdev(scull_p_devices + i, i);
   }
#ifdef SCULL_DEBUG
   create_proc_read_entry("scullpipe", 0, NULL, scull_read_p_mem, NULL);
#endif
   return scull_p_nr_devs;
}

/*
 * This is called by cleanup_module or on failure.
 * It is required to never fail, even if nothing was initialized first
 */
void scull_p_cleanup(void) {
   int i;
   
#ifdef SCULL_DEBUG
   remove_proc_entry("scullpipe", NULL);
#endif
   
   if (!scull_p_devices) return; /* nothing else to release */
   
   for (i = 0; i < scull_p_nr_devs; i++) {
      cdev_del(&scull_p_devices[i].cdev);
      kfree(scull_p_devices[i].buffer);
   }
   kfree(scull_p_devices);
   unregister_chrdev_region(scull_p_devno, scull_p_nr_devs);
   scull_p_devices = NULL;
}
