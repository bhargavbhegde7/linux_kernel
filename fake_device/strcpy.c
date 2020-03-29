#include <stdio.h>
#include <stddef.h>

// Function to implement strncpy() function
char* strncpy(char* destination, const char* source, size_t num)
{
	// return if no memory is allocated to the destination
	if (destination == NULL)
		return NULL;

	// take a pointer pointing to the beginning of destination string
	char* ptr = destination;

	// copy first num characters of C-string pointed by source
	// into the array pointed by destination
	while (*source && num--)
	{
		*destination = *source;
		destination++;
		source++;
	}

	// null terminate destination string
	*destination = '\0';

	// destination is returned by standard strncpy()
	return ptr;
}

// Implement strncpy function in C
int main(void)
{
	char* source = "Techie Delight";
	char destination[20];

	size_t num = 6;

	// Copies the first num characters of source to destination
	printf("%s", strncpy(destination, source, num));

	return 0;
}