#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>


int main(int argc, char **argv){
	
	int fbfd = 0;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	long int screensize = 0;
	char *fbp = 0;

	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd == -1){
		perror("ERROR: cannot open framebuffer device");
		exit(1);
	}
	printf("The framebuffer device was opened successfully.\n");

	if(ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1){
		perror("ERROR: Reading fixed information");
		exit(2);
	}

	if(ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1){
		perror("ERROR: Reading variable information");
		exit(3);
	}

	printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

	screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

	fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	if((int)fbp == -1){
		perror("ERROR: failed to map framebuffer device to memory");
		exit(4);
	}

	printf("The framebuffer device was mapped to memory successfully.\n");

	munmap(fbp, screensize);
	close(fbfd);
	return 0;
}
