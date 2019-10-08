#include<linux/module.h>  //needed for module_init and module_exit
#include<linux/kernel.h>  //needed for KERN_INFO
#include<linux/init.h>        //needed for the macros
#include<linux/sched.h> // for getting the current process and the pointer to 'task_struct'
#include<linux/moduleparam.h>

static char *whom = "world";
static int how_many = 1;
module_param(how_many, int, S_IRUGO);
module_param(whom, charp, S_IRUGO);


int __init hw_init(void) {
    
	int i;
	for(i = 0; i < how_many; i++){
		printk(KERN_INFO"Hello %s\n", whom);
	}

	printk(KERN_INFO "The process is \"%s\" (pid %i)\n", current->comm, current->pid);
    return 0;
}
 
void __exit hw_exit(void) {
        printk(KERN_INFO"Bye World\n");
}
 
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("bhargav");
MODULE_DESCRIPTION("dummy module to test");
 
module_init(hw_init);
module_exit(hw_exit);

/* https://linuxhint.com/linux-device-driver-tutorial  */
