//
// Created by thevoidnn on 10/25/17.
//

#ifndef ARDUINO_USBIP_USBIP_DEFS_H
#define ARDUINO_USBIP_USBIP_DEFS_H

#include <cstdint>
#include "usb_defs.h"

#define USBIP_SYSFS_PATH_SIZE 256
#define USBIP_BUSID_SIZE 32

enum usbip_stage1_command {
    USBIP_STAGE1_CMD_DEVICE_LIST   = 0x05,
    USBIP_STAGE1_CMD_DEVICE_ATTACH = 0x03,
};

enum usbip_stager2_command {
    USBIP_STAGE2_REQ_SUBMIT = 0x0001,
    USBIP_STAGE2_REQ_UNLINK = 0x0002,
    USBIP_STAGE2_RSP_SUBMIT = 0x0003,
    USBIP_STAGE2_RSP_UNLINK = 0x0004,
};

enum usbip_stage2_direction {
    USBIP_DIR_OUT = 0x00,
    USBIP_DIR_IN  = 0x01,
};

struct usbip_stage1_header {
    uint16_t version;
    uint16_t command;
    uint32_t  status;
} __attribute__ ((__packed__));



struct usbip_stage1_usb_device {
    char path[USBIP_SYSFS_PATH_SIZE];
    char busid[USBIP_BUSID_SIZE];

    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;

    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;

    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;

    uint8_t bConfigurationValue;
    uint8_t bNumConfigurations;
    uint8_t bNumInterfaces;
} __attribute__((packed));

struct usbip_stage1_usb_interface {
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t padding;
} __attribute__((packed));

struct usbip_stage1_response_devlist_entry {
    struct usbip_stage1_usb_device    udev;
    struct usbip_stage1_usb_interface uinf[];
} __attribute__((packed));

struct usbip_stage1_response_devlist {
    uint32_t  list_size;
    usbip_stage1_response_devlist_entry devices[];
} __attribute__ ((__packed__));













/**
 * struct usbip_header_basic - data pertinent to every request
 * @command: the usbip request type
 * @seqnum: sequential number that identifies requests; incremented per
 *	    connection
 * @devid: specifies a remote USB device uniquely instead of busnum and devnum;
 *	   in the stub driver, this value is ((busnum << 16) | devnum)
 * @direction: direction of the transfer
 * @ep: endpoint number
 */
struct usbip_stage2_header_basic {
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} __attribute__((packed));

/**
 * struct usbip_header_cmd_submit - USBIP_CMD_SUBMIT packet header
 * @transfer_flags: URB flags
 * @transfer_buffer_length: the data size for (in) or (out) transfer
 * @start_frame: initial frame for isochronous or interrupt transfers
 * @number_of_packets: number of isochronous packets
 * @interval: maximum time for the request on the server-side host controller
 * @setup: setup data for a control request
 */
struct usbip_stage2_header_cmd_submit {
    uint32_t transfer_flags;
    int32_t data_length;

    /* it is difficult for usbip to sync frames (reserved only?) */
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t interval;

    union {
        uint8_t setup[8];
        usb_standard_request request;
    };
} __attribute__((packed));

/**
 * struct usbip_header_ret_submit - USBIP_RET_SUBMIT packet header
 * @status: return status of a non-iso request
 * @actual_length: number of bytes transferred
 * @start_frame: initial frame for isochronous or interrupt transfers
 * @number_of_packets: number of isochronous packets
 * @error_count: number of errors for isochronous transfers
 */
struct usbip_stage2_header_ret_submit {
    int32_t status;
    int32_t data_length;
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t error_count;
} __attribute__((packed));

/**
 * struct usbip_header_cmd_unlink - USBIP_CMD_UNLINK packet header
 * @seqnum: the URB seqnum to unlink
 */
struct usbip_stage2_header_cmd_unlink {
    uint32_t seqnum;
} __attribute__((packed));

/**
 * struct usbip_header_ret_unlink - USBIP_RET_UNLINK packet header
 * @status: return status of the request
 */
struct usbip_stage2_header_ret_unlink {
    int32_t status;
} __attribute__((packed));

/**
 * struct usbip_header - common header for all usbip packets
 * @base: the basic header
 * @u: packet type dependent header
 */
struct usbip_stage2_header {
    struct usbip_stage2_header_basic base;

    union {
        struct usbip_stage2_header_cmd_submit	cmd_submit;
        struct usbip_stage2_header_ret_submit	ret_submit;
        struct usbip_stage2_header_cmd_unlink	cmd_unlink;
        struct usbip_stage2_header_ret_unlink	ret_unlink;
    } u;
} __attribute__((packed));







#endif //ARDUINO_USBIP_USBIP_DEFS_H
