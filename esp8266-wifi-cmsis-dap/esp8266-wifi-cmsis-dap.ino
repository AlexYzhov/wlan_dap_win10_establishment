
/* CMSIS-DAP ported to run on the Pro Micro and Teensy 3.2
 * Copyright (C) 2016 Phillip Pearson <pp@myelin.co.nz>
 *
 * CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * *******************************************************************
 *

SWD PINOUT:

SWD_CLOCK: D3  (0)
SWD_DATA:  D4  (2)
SWD_VDD:   3V
SWD_GND:   GND

(see DAP_config.h line 127)

 */

#include <ESP8266WiFi.h>

#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)


#define USB_DEVICE_VENDOR_ID  0x16c0
#define USB_DEVICE_PRODUCT_ID 0x05df
#define USB_DEVICE_VERSION    0x0130

#define USB_HID_MAX_PACKET_SIZE 64

#include "DAP_config.h"
#include "DAP.h"


#include "usb_defs.h"
#include "usbip_defs.h"

// wifi setup:

const char* ssid = "DRCCCP";
const char* pass = "";



const char *strings[] = {
        0, // reserved: available languages
        "thevoidnn",
        "esp8266 CMSIS-DAP",
        "1234",
};

const uint8_t hid_report_descriptor[] = {
        0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
        0x09, 0x01,        // Usage (0x01)
        0xA1, 0x01,        // Collection (Application)
        0x15, 0x00,        //   Logical Minimum (0)
        0x26, 0xFF, 0x00,  //   Logical Maximum (255)
        0x75, 0x08,        //   Report Size (8)
        0x95, 0x40,        //   Report Count (64)
        0x09, 0x01,        //   Usage (0x01)
        0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
        0x95, 0x40,        //   Report Count (64)
        0x09, 0x01,        //   Usage (0x01)
        0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0x95, 0x01,        //   Report Count (1)
        0x09, 0x01,        //   Usage (0x01)
        0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
        0xC0,              // End Collection

        // 33 bytes
};



#define STR_ILANGUAGES_SUPPORTED 0
#define STR_IMANUFACTURER 1
#define STR_IPRODUCT      2
#define STR_ISERIAL       3

#define USBIP_SERVER_PORT 3240






void dump_buff(void *data, int size) {
    Serial.println("****************************");
    uint8_t *buffer = (uint8_t *)data;
    for (int i = 0; i < size; i++) {
        if (i != 0 && i % 16 == 0) {
           Serial.println();
        }
        unsigned int val = buffer[i];
        Serial.print(val, HEX);
        Serial.print(' ');
    }
    Serial.println();
    Serial.println("****************************");
    Serial.println();
}


class USBIPServer {
public:

    enum state_t {
        ACCEPTING,
        ATTACHING,
        EMULATING,
    };

    typedef int (*callback_t)(void *data_in, void *data_out);

    USBIPServer(callback_t on_hid_data)
        : respond(false)
        , state(ACCEPTING)
        , on_hid_data(on_hid_data)
        , wifiServer(USBIP_SERVER_PORT)
    {
        memset(data_in, 0, USB_HID_MAX_PACKET_SIZE);
        memset(data_out, 0, USB_HID_MAX_PACKET_SIZE);
    }

    void begin() {
        wifiServer.begin();
        wifiServer.setNoDelay(true);
    }


    void loop() {
        //Serial.println("loop");

        switch (state) {
        case ACCEPTING:
            accept();
            break;

        case ATTACHING:
            attach();
            break;

        case EMULATING:
            emulate();
            break;

        }

        if (!client.connected()) {
            state = ACCEPTING;
        }

    }

    void accept() {
        if (wifiServer.hasClient()) {
            client = wifiServer.available();
            state = ATTACHING;
        }
    }

    void attach() {
        int command = read_stage1_command();
        if (command < 0) {
            return;
        }

        switch (command) {
        case USBIP_STAGE1_CMD_DEVICE_LIST:
            handle_device_list();
            break;

        case USBIP_STAGE1_CMD_DEVICE_ATTACH:
            handle_device_attach();
            break;

        default:
            Serial.println("s1 unknown command:");
            Serial.println(command);
            break;
        }
    }

    void emulate() {
        usbip_stage2_header header;
        int command = read_stage2_command(header);
        if (command < 0) {
            return;
        }

        switch (command) {
        case USBIP_STAGE2_REQ_SUBMIT:
            handle_submit(header);
            break;

        case USBIP_STAGE2_REQ_UNLINK:
            handle_unlink(header);
            break;

        default:
            Serial.println("s2 unknown command:");
            Serial.println(command);
        }
    }


protected:

    void write(void *data, int size) {
        client.write((uint8_t *)data, size);
    }

    void send_stage2_submit(usbip_stage2_header &req_header, int32_t status, int32_t data_length) {

        req_header.base.command = USBIP_STAGE2_RSP_SUBMIT;
        req_header.base.direction = !req_header.base.direction;

        memset(&req_header.u.ret_submit, 0, sizeof(usbip_stage2_header_ret_submit));

        req_header.u.ret_submit.status = status;
        req_header.u.ret_submit.data_length = data_length;

        pack(&req_header, sizeof(usbip_stage2_header));

        write(&req_header, sizeof(usbip_stage2_header));        
    }

    void send_stage2_submit_data(usbip_stage2_header &req_header, int32_t status, void *data, int32_t data_length) {

        send_stage2_submit(req_header, status, data_length);

        if (data_length) {
            write(data, data_length);
        }
    }

    void send_stage2_unlink(usbip_stage2_header &req_header) {

        req_header.base.command = USBIP_STAGE2_RSP_UNLINK;
        req_header.base.direction = USBIP_DIR_OUT;

        memset(&req_header.u.ret_unlink, 0, sizeof(usbip_stage2_header_ret_unlink));

        // req_header.u.ret_unlink.status = 0;

        pack(&req_header, sizeof(usbip_stage2_header));

        write(&req_header, sizeof(usbip_stage2_header));
    }


    void send_stage1_header(uint16_t command, uint32_t status) {
        Serial.println("s1 sending header...");
        usbip_stage1_header header;
        header.version = htons(273);
        header.command = htons(command);
        header.status  = htonl(status);

        write(&header, sizeof(usbip_stage1_header));
    }

    void send_device_list() {
        Serial.println("s1 sending device list...");

        // send device list size:
        Serial.println("s1 sending device list size...");
        usbip_stage1_response_devlist response_devlist;

        // we have only 1 device, so:
        response_devlist.list_size = htonl(1);


        write(&response_devlist, sizeof(usbip_stage1_response_devlist));

        // may be foreach:

        {
            // send device info:

            send_device_info();

            // send device interfaces: // (1)

            send_interface_info();
        }

    }

    void send_device_info() {
        Serial.println("s1 sending device info...");
        usbip_stage1_usb_device device;

        strcpy(device.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
        strcpy(device.busid, "1-1");

        device.busnum = htonl(1);
        device.devnum = htonl(2);
        device.speed  = htonl(2);

        // see: https://github.com/obdev/v-usb/blob/master/usbdrv/USB-IDs-for-free.txt
        device.idVendor  = htons(USB_DEVICE_VENDOR_ID);
        device.idProduct = htons(USB_DEVICE_PRODUCT_ID);
        // device version:
        device.bcdDevice = htons(USB_DEVICE_VERSION);

        device.bDeviceClass    = 0x00; // <defined at interface level>
        device.bDeviceSubClass = 0x00;
        device.bDeviceProtocol = 0x00;

        device.bConfigurationValue = 1;
        device.bNumConfigurations = 1;
        device.bNumInterfaces = 1;

        write(&device, sizeof(usbip_stage1_usb_device));
    }

    void send_interface_info() {
        Serial.println("sending interface info...");
        usbip_stage1_usb_interface interface;
        interface.bInterfaceClass    = USB_CLASS_HID;
        interface.bInterfaceSubClass = 0;
        interface.bInterfaceProtocol = 0;
        interface.padding = 0;

        write(&interface, sizeof(usbip_stage1_usb_interface));
    }


    int read_stage1_command() {
        if (client.available() < sizeof(usbip_stage1_header)) {
            return -1;
        }

        usbip_stage1_header req;

        client.readBytes((uint8_t *)&req, sizeof(usbip_stage1_header));
        return ntohs(req.command) & 0xFF; // 0x80xx
    }

    int read_stage2_command(usbip_stage2_header &header) {
        if (client.available() < sizeof(usbip_stage2_header)) {
            return -1;
        }

        client.readBytes((uint8_t *)&header, sizeof(usbip_stage2_header));
        unpack((uint32_t *) &header, sizeof(usbip_stage2_header));
        return header.base.command;
    }


    void pack(void* data, int size) {

        // don't fk with setup bytes
        int sz = (size/sizeof(uint32_t)) - 2;
        uint32_t *ptr = (uint32_t *)data;

        for(int i = 0; i < sz; i++) {
            //Serial.print(i);
            ptr[i]=htonl(ptr[i]);
        }
    }

    void unpack(void* data, int size) {

        // don't fk with setup bytes
        int sz = (size/sizeof(uint32_t)) - 2;
        uint32_t *ptr = (uint32_t *)data;

        for(int i = 0; i < sz; i++) {
            ptr[i]=ntohl(ptr[i]);
        }
    }


    // handlers:

    // stage 1:

    void handle_device_list() {
        Serial.println("s1 handling dev list request...");

        send_stage1_header(USBIP_STAGE1_CMD_DEVICE_LIST, 0);

        send_device_list();
    }

    void handle_device_attach() {
        Serial.println("s1 handling dev attach request...");

        char bus[USBIP_BUSID_SIZE];
        while (client.available() < USBIP_BUSID_SIZE) {
            // nop
        }
        client.readBytes((uint8_t *)bus, USBIP_BUSID_SIZE);

        send_stage1_header(USBIP_STAGE1_CMD_DEVICE_ATTACH, 0);

        send_device_info();

        state = EMULATING;
    }

    // stage 2:

    void handle_submit(usbip_stage2_header &header) {
        //dump_cmd_submit(header);

        switch (header.base.ep) {
        // control
        case 0x00:
            handle_control_request(header);
            break;

        // data
        case 0x01:
            if (header.base.direction == 0) {
                // Serial.println("EP 01 DATA FROM HOST");
                handle_data_request(header);
            } else {
                // Serial.println("EP 01 DATA TO HOST");
                handle_data_response(header);
            }
            break;

        // request to save data to device
        case 0x81:
            if (header.base.direction == 0) {
                Serial.println("*** WARN! EP 81 DATA TX");
            } else {
                Serial.println("*** WARN! EP 81 DATA RX");
            }
            break;

        default:
            Serial.print("*** WARN ! UNKNOWN ENDPOINT: ");
            Serial.println((int)header.base.ep);
            //usleep(10000000);
        }

    }

    void handle_unlink(usbip_stage2_header &header) {
        Serial.println("s2 handling cmd unlink...");
        send_stage2_unlink(header);
    }


    // stage 2 helpers:

    void handle_control_request(usbip_stage2_header &header) {

        switch (header.u.cmd_submit.request.bmRequestType) {
        case 0x80: // : default
        case 0x81: // : hid
            switch (header.u.cmd_submit.request.bRequest) {
            case 0x06: // GET DESCRIPTOR
                handle_get_descriptor(header);
                break;

            default:
                Serial.println("usb unknown request:");
                Serial.println(header.u.cmd_submit.request.bRequest);
                break;
            }
            break;


        case 0x00:
            switch (header.u.cmd_submit.request.bRequest) {
            case 0x09: // SET CONFIGURATION
                handle_set_configuration(header);
                break;

            default:
                Serial.println("usb unknown request:");
                Serial.println(header.u.cmd_submit.request.bRequest);
                break;
            }
            break;

        case 0x21:
            switch (header.u.cmd_submit.request.bRequest) {
            case 0x0a:
                handle_set_idle(header);
                break;

            default:
                Serial.println("usb unknown request:");
                Serial.println(header.u.cmd_submit.request.bRequest);
                break;
            }
            break;

        default:
            Serial.println("usb unknown request type:");
            Serial.println(header.u.cmd_submit.request.bmRequestType);
            break;
        }
    }

    void handle_data_request(usbip_stage2_header &header) {
        while (client.available() < USB_HID_MAX_PACKET_SIZE) {
            // nop
        }
        client.readBytes(data_in, USB_HID_MAX_PACKET_SIZE);

        respond = on_hid_data(data_in, data_out);

        send_stage2_submit(header, 0, 0);
    }

    void handle_data_response(usbip_stage2_header &header) {
        if (respond) {
            respond = false;
            //Serial.println("*** Will respond");
            send_stage2_submit_data(header, 0, data_out, USB_HID_MAX_PACKET_SIZE);
            //Serial.println("*** RESPONDED ***");
        } else {
            //Serial.println("*** Will NOT respond");
            send_stage2_submit(header, 0, 0);
        }
    }


    void handle_get_descriptor(usbip_stage2_header &header) {

        switch (header.u.cmd_submit.request.wValue.u8hi) {
        case USB_DT_DEVICE:
            handle_get_device_descriptor(header);
            break;

        case USB_DT_CONFIGURATION:
            handle_get_configuration_descriptor(header);
            break;

        case USB_DT_STRING:
            handle_get_string_descriptor(header);
            break;

        case USB_DT_INTERFACE:
            handle_get_interface_descriptor(header);
            break;

        case USB_DT_ENDPOINT:
            handle_get_endpoint_descriptor(header);
            break;

        case USB_DT_DEVICE_QUALIFIER:
            handle_get_device_qualifier_descriptor(header);
            break;

        case USB_DT_OTHER_SPEED_CONFIGURATION:
            Serial.println("GET 0x07 [UNIMPLEMENTED] USB_DT_OTHER_SPEED_CONFIGURATION");
            break;

        case USB_DT_INTERFACE_POWER:
            Serial.println("GET 0x08 [UNIMPLEMENTED] USB_DT_INTERFACE_POWER");
            break;

        case USB_DT_REPORT:
            handle_get_hid_report_descriptor(header);
            break;

        default:
            Serial.println("usb unknown descriptor requested:");
            Serial.println(header.u.cmd_submit.request.wValue.u8lo);
            break;
        }
    }

    void handle_get_device_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x01 DEVICE DESCRIPTOR");

        usb_device_descriptor desc;

        desc.bLength = USB_DT_DEVICE_SIZE;
        desc.bDescriptorType = USB_DT_DEVICE;

        desc.bcdUSB = 0x0110;

        // defined at interface level
        desc.bDeviceClass    = 0x0;
        desc.bDeviceSubClass = 0x0;
        desc.bDeviceProtocol = 0x0;

        desc.bMaxPacketSize0 = USB_HID_MAX_PACKET_SIZE;

        desc.idVendor  = USB_DEVICE_VENDOR_ID;
        desc.idProduct = USB_DEVICE_PRODUCT_ID;
        desc.bcdDevice = USB_DEVICE_VERSION;

        desc.iManufacturer = STR_IMANUFACTURER;
        desc.iProduct      = STR_IPRODUCT;
        desc.iSerialNumber = STR_ISERIAL;

        desc.bNumConfigurations = 1;

        send_stage2_submit_data(header, 0, &desc, sizeof(usb_device_descriptor));
    }

    void handle_get_configuration_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x02 CONFIGURATION DESCRIPTOR");

        if (header.u.cmd_submit.data_length == USB_DT_CONFIGURATION_SIZE) {
            Serial.println("Sending only first part of CONFIG");

            // USB_DT_CONFIGURATION_SIZE + USB_DT_INTERFACE_SIZE + USB_DT_HID_SIZE + USB_DT_REPORT_SIZE + (2 * USB_DT_ENDPOINT_SIZE)
            send_stage2_submit(header, 0, header.u.cmd_submit.data_length);

            send_stage2_configuration_descriptor();
        } else {
            Serial.println("Sending ALL CONFIG");
            // USB_DT_CONFIGURATION_SIZE + USB_DT_INTERFACE_SIZE + USB_DT_HID_SIZE + USB_DT_REPORT_SIZE + (2 * USB_DT_ENDPOINT_SIZE)
            send_stage2_submit(header, 0, header.u.cmd_submit.data_length);

            send_stage2_configuration_descriptor();
            send_stage2_interface_descriptor();
            send_stage2_hid_descriptor();
            send_stage2_endpoint_descriptors();
        }
    }

    void handle_get_string_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x03 STRING DESCRIPTOR");

        if (header.u.cmd_submit.request.wValue.u8lo == 0) {
            Serial.println("** REQUESTED list of supported languages");

            uint8_t lng[4];
            usb_string_descriptor *lng_desc = (usb_string_descriptor *)lng;
            lng_desc->bLength = 0x04;
            lng_desc->bDescriptorType = USB_DT_STRING;
            lng_desc->wData[0] = 0x0409;

            send_stage2_submit_data(header, 0, lng, 0x04);

            return;
        }

        Serial.println("** REQUESTED STRING ID:");
        Serial.println(header.u.cmd_submit.request.wValue.u8lo);
        Serial.println("** SENDING STRING:");
        Serial.println(strings[header.u.cmd_submit.request.wValue.u8lo]);

        int slen = strlen(strings[header.u.cmd_submit.request.wValue.u8lo]);
        int wslen = slen * 2;
        int buff_len = sizeof(usb_string_descriptor) + wslen;

        uint8_t *buff = new uint8_t[buff_len];
        usb_string_descriptor *desc = (usb_string_descriptor *)buff;

        desc->bLength = buff_len;
        desc->bDescriptorType = USB_DT_STRING;
        for (int i = 0; i < slen; i++) {
            desc->wData[i] = strings[header.u.cmd_submit.request.wValue.u8lo][i];
        }

        send_stage2_submit_data(header, 0, buff, buff_len);

        delete[] buff;
    }

    void handle_get_interface_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x04 INTERFACE DESCRIPTOR (UNIMPLEMENTED)");

        send_stage2_submit(header, 0, 0);
    }

    void handle_get_endpoint_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x05 ENDPOINT DESCRIPTOR (UNIMPLEMENTED)");

    }

    void handle_get_device_qualifier_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x06 DEVICE QUALIFIER DESCRIPTOR");

        usb_device_qualifier_descriptor desc;

        memset(&desc, 0, sizeof(usb_device_qualifier_descriptor));

        send_stage2_submit_data(header, 0, &desc, sizeof(usb_device_qualifier_descriptor));
    }

    void handle_get_hid_report_descriptor(usbip_stage2_header &header) {
        Serial.println("* GET 0x22 HID REPORT DESCRIPTOR");

        send_stage2_submit_data(header, 0, (void *)hid_report_descriptor, sizeof(hid_report_descriptor));
    }


    void handle_set_configuration(usbip_stage2_header &header) {
        Serial.println("* SET CONFIGURATION");
        send_stage2_submit_data(header, 0, 0, 0);
    }

    void handle_set_idle(usbip_stage2_header &header) {
        Serial.println("* SET IDLE");
        send_stage2_submit(header, 0, 0);
    }




    void send_stage2_configuration_descriptor() {
        usb_config_descriptor desc;

        desc.bLength = USB_DT_CONFIGURATION_SIZE;
        desc.bDescriptorType = USB_DT_CONFIGURATION;

        desc.wTotalLength = USB_DT_CONFIGURATION_SIZE + USB_DT_INTERFACE_SIZE + USB_DT_HID_SIZE + USB_DT_REPORT_SIZE + (2 * USB_DT_ENDPOINT_SIZE);
        desc.bNumInterfaces = 1;
        desc.bConfigurationValue = 1;
        desc.iConfiguration = 0;
        desc.bmAttributes = 0x80;// 0xC0; ?
        desc.bMaxPower = 0x50;

        write(&desc, USB_DT_CONFIGURATION_SIZE);
    }

    void send_stage2_interface_descriptor() {

        usb_interface_descriptor desc;

        desc.bLength = USB_DT_INTERFACE_SIZE;
        desc.bDescriptorType = USB_DT_INTERFACE;

        desc.bInterfaceNumber = 0;
        desc.bAlternateSetting = 0;
        desc.bNumEndpoints = 2;

        desc.bInterfaceClass = USB_CLASS_HID;
        desc.bInterfaceSubClass = 0;
        desc.bInterfaceProtocol = 0;

        desc.iInterface = 0;

        write(&desc, USB_DT_INTERFACE_SIZE);
    }

    void send_stage2_hid_descriptor() {

        usb_hid_descriptor desc;

        desc.bLength = USB_DT_HID_SIZE + USB_DT_REPORT_SIZE;
        desc.bDescriptorType = USB_DT_HID;

        desc.bcdHID = 0x001;// 0x0111; ?
        desc.bCountryCode = 0;
        desc.bNumDescriptors = 1;

        write(&desc, USB_DT_HID_SIZE);

        usb_hid_report_descriptor rep_desc;
        rep_desc.bDescriptorType = USB_DT_REPORT;
        rep_desc.wReportLength = sizeof(hid_report_descriptor);

        write(&rep_desc, USB_DT_REPORT_SIZE);
    }

    void send_stage2_endpoint_descriptors() {
        usb_endpoint_descriptor hid_endpoints[] = {
                {
                        .bLength = USB_DT_ENDPOINT_SIZE,
                        .bDescriptorType = USB_DT_ENDPOINT,
                        .bEndpointAddress = 0x81,
                        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
                        .wMaxPacketSize = USB_HID_MAX_PACKET_SIZE,
                        .bInterval = 0xff,
                },
                {
                        .bLength = USB_DT_ENDPOINT_SIZE,
                        .bDescriptorType = USB_DT_ENDPOINT,
                        .bEndpointAddress = 0x01,
                        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
                        .wMaxPacketSize = USB_HID_MAX_PACKET_SIZE,
                        .bInterval = 0xff,
                },
        };

        write(hid_endpoints, sizeof(hid_endpoints));
    }

private:

    uint8_t data_in[USB_HID_MAX_PACKET_SIZE];
    uint8_t data_out[USB_HID_MAX_PACKET_SIZE];

    bool respond;

    state_t state;

    callback_t on_hid_data;

    WiFiServer wifiServer;

    WiFiClient client;
};


int on_hid_data(void *data_in, void *data_out) {
    //Serial.print('.');
    
    int res = DAP_ProcessCommand((uint8_t *)data_in, (uint8_t *)data_out);

    //dump_buff(data_in, USB_HID_MAX_PACKET_SIZE);
    
    //dump_buff(data_out, USB_HID_MAX_PACKET_SIZE);
    
    return res;
}

USBIPServer server(on_hid_data);

void setup() {

    Serial.begin(115200);
    Serial.println();
    Serial.println("Starting WiFi...");

    WiFi.begin(ssid, pass);
    do {
        delay(500);
    } while (WiFi.status() != WL_CONNECTED);
    
    Serial.println("Initializing DAP...");
    DAP_Setup();

    Serial.println("Starting USBIP server...");
    server.begin();
    
}

void loop() {
    server.loop();
}




/*
void setup() {
  Serial.begin(115200);

  DAP_Setup();

#ifdef HIDPROJECT_RAWHID
  // Set the RawHID OUT report array.
  // Feature reports are also (parallel) possible, see the other example for this.
  RawHID.begin(rawhidRequest, DAP_PACKET_SIZE);
#endif
}


void loop() {
  // Check if there is new data from the RawHID device
  auto bytesAvailable =
#ifdef HIDPROJECT_RAWHID
    RawHID.available();
#else
    RawHID.recv(rawhidRequest, 0);
#endif
  if (bytesAvailable > 0) {
    Serial.print("cmd ");
    Serial.print(rawhidRequest[0], HEX);
    Serial.print(" ");
    Serial.print(rawhidRequest[1], HEX);
    Serial.print(" ");
    auto sz = DAP_ProcessCommand(rawhidRequest, rawhidResponse);
    Serial.print("rsp ");
    Serial.print(sz);
    Serial.println(" B");
#ifdef HIDPROJECT_RAWHID
    RawHID.enable(); // signal that we're ready to receive another buffer
#endif
    if (sz > 0) {
#ifdef HIDPROJECT_RAWHID
      RawHID.write(rawhidResponse, DAP_PACKET_SIZE);
#else
      RawHID.send(rawhidResponse, DAP_PACKET_SIZE);
#endif
    }
  }
}
*/
