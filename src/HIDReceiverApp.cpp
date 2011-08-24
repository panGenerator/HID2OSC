/*******************************************************
 Copyright (C) <year> by <copyright holders>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*******************************************************/

/*
 The following source code demonstrates handling input from USB HID NES controllers
 and translating it to OSC messages using two libraries
 + The Cinder shipped OSC package
 + The most super-awesome HIDAPI https://github.com/signal11/hidapi
*/

#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"

#include "OscSender.h"

#include "hidapi.h"
#include <iostream>
#include <bitset>

#define PAD_COUNT 2

/**
 * Specific for TOMee NES USB CONTROLLER. Change for Your device.
 * If You want to find the VENDOR_ID and PRODUCT_ID uncomment the 
 * call to __hidDEBUG() in setup function - it'll list all the devices
 * plugged in
 */
#define PAD_VENDOR_ID 0x12bd
#define PAD_PRODUCT_ID 0xd015
#define PAD_KEYS_COUNT 8

/**
 * padKeyStates array indices. Only 8 because NES controller doesn't have any more buttons
 */
#define PAD_KEY_UP 0
#define PAD_KEY_DOWN 1
#define PAD_KEY_LEFT 2
#define PAD_KEY_RIGHT 3
#define PAD_KEY_SELECT 4
#define PAD_KEY_START 5
#define PAD_KEY_A 6
#define PAD_KEY_B 7


using namespace ci;
using namespace ci::app;
using namespace std;

class HIDReceiverApp : public AppBasic {
  public:
	void setup();
	void initHIDPads();
	void __hidDEBUG();
	void update();
	void updateKeyStates();
	void setPadKeyState( int pad , int key , bool isPressed );
	void sendKeyStateAsOSCMessage(int pad , int key , bool isPressed );
	void draw();
	void int_bin(int f);
	
  private:
	Color c;
	/**
	 * HID communication buffer
	 */
	unsigned char buf[256];
	/**
	 * 
	 */
	int padsFoundCount;	
	/**
	 * Current key states
	 */
	bool padKeyStates[PAD_COUNT][PAD_KEYS_COUNT];
	/**
	 *
	 */
	hid_device *pads[PAD_COUNT];
	/**
	 *
	 */
	osc::Sender sender;
	/**
	 *
	 */
	std::string oscHost;
	int oscPort;
};


void HIDReceiverApp::setup(){
	//__hidDEBUG();
	
	initHIDPads();
	
	
	//Broadcast... Monica Broadcast
	oscHost = "255.255.255.255";
	oscPort = 10000;
	sender.setup(oscHost,oscPort);
}

/**
 *
 */
void HIDReceiverApp::update(){	
	updateKeyStates();
	c = Color( padKeyStates[0][0]==true?1:0,padKeyStates[0][1]==true?1:0,padKeyStates[0][3]==true?1:0);
}

/**
 * Read input from HID devices which are being monitored (NES pads)
 */
void HIDReceiverApp::updateKeyStates(){
	if( padsFoundCount > 0 ){
		hid_device *handle;
		//read HID data
		for( int p = 0 ; p < padsFoundCount ; p++ ){
			handle = pads[p];
			//console() << handle << std::endl;
			
			int res = hid_read(handle, buf, sizeof(buf));
			if (res == 0){
				; //nop
			}else if (res < 0){
				printf("Unable to read()\n");			
			}else{
				
				/**
				 * Printing the lines below helps understand the code which is executed after
				 */
				/*
				 console() << "Pad #" << p << " data read = \n";
				 for (int i = 0; i < res; i++){
					printf("%d: %d / %02hhx ", i , buf[i] , buf[i]);
					console() << " ( ";
					int_bin( buf[i] );
					console() << " ) " << std::endl;
					//padsStates[p][i] = buf[i];
				}
				printf("-----------------------------------\n");			
				*/
				
				/**
				 * ...yup, this stuff - which is mapping the incomming data to padState array
				 */
				//UP - DOWN
				if( buf[1] == 0 ){	//0x00
					console() << "Pad up pressed" << std::endl;
					setPadKeyState(p, PAD_KEY_UP, true);
				}
				else if( buf[1] == 255 ){	//0xFF
					console() << "Pad down pressed" << std::endl;
					setPadKeyState(p, PAD_KEY_DOWN, true);
				}
				else{	//0xF0
					setPadKeyState(p, PAD_KEY_UP, false);
					setPadKeyState(p, PAD_KEY_DOWN, false);
				}
				
				//LEFT - RIGHT
				if( buf[0] == 0 ){		//0x00
					console() << "Pad up pressed" << std::endl;
					setPadKeyState(p, PAD_KEY_LEFT, true);
				}
				else if( buf[0] == 255 ){	//0xFF
					console() << "Pad down pressed" << std::endl;
					setPadKeyState(p, PAD_KEY_RIGHT, true);
				}
				else{	//0xF0
					setPadKeyState(p, PAD_KEY_LEFT, false);
					setPadKeyState(p, PAD_KEY_RIGHT, false);
				}
				
				//SELECT+START - can be pressed together => bit glueing
				if( buf[4] == 1 ){	//0b01 (select)
					setPadKeyState(p, PAD_KEY_SELECT, true);
					setPadKeyState(p, PAD_KEY_START, false);
					
				}
				else if( buf[4] == 2 ){	//0b10 (start)
					setPadKeyState(p, PAD_KEY_SELECT, false);
					setPadKeyState(p, PAD_KEY_START, true);
				}
				else if( buf[4] == 3 ){	 //0b11 (select+start)
					setPadKeyState(p, PAD_KEY_SELECT, true);
					setPadKeyState(p, PAD_KEY_START, true);
				}
				else{	//0b00	(none)
					setPadKeyState(p, PAD_KEY_SELECT, false);
					setPadKeyState(p, PAD_KEY_START, false);
				}

				//A+B - can be pressed together => bit glueing
				if( buf[3] == 1 ){	//0b01 (select)
					setPadKeyState(p, PAD_KEY_A, true);
					setPadKeyState(p, PAD_KEY_B, false);
				}
				else if( buf[3] == 2 ){	//0b10 (start)
					setPadKeyState(p, PAD_KEY_A, false);					
					setPadKeyState(p, PAD_KEY_B, true);
				}
				else if( buf[3] == 3 ){	 //0b11 (select+start)
					setPadKeyState(p, PAD_KEY_A, true);
					setPadKeyState(p, PAD_KEY_B, true);
				}
				else{	//0b00	(none)
					setPadKeyState(p, PAD_KEY_A, false);
					setPadKeyState(p, PAD_KEY_B, false);
				}				
				
			}
			
		}

	}
}

/**
 * Set the key state and send an OSC message if the state changed
 * @param	pad				pad id
 * @param	key				Key id use the PAD_KEY_* constants
 * @param	isPressed		if the button is pressed
 */
void HIDReceiverApp::setPadKeyState( int pad , int key , bool isPressed ){
	if( isPressed != padKeyStates[pad][key] ){
		sendKeyStateAsOSCMessage(pad, key, isPressed);
	}
	padKeyStates[pad][key] = isPressed;
}

/**
 * Send the key state as an OSC message
 * @param	pad				pad id
 * @param	key				Key id use the PAD_KEY_* constants
 * @param	isPressed		if the button is pressed
 */
void HIDReceiverApp::sendKeyStateAsOSCMessage(int pad , int key , bool isPressed ){
	//console() << "Pad: " << pad << ", key: " << key << " sendingOSC state: " << isPressed << std::endl;
	
	int isPressedInt = isPressed==true?1:0;
	
	osc::Message message;
	message.addIntArg(pad);
	message.addIntArg(key);
	message.addIntArg(isPressedInt);
	message.setAddress("/state");
	message.setRemoteEndpoint(oscHost, oscPort);
	sender.sendMessage(message);	
}

/**
 * Heavy stuff!
 */
void HIDReceiverApp::draw(){
	gl::clear( c ); 
}

/**
 * Initialize pads via HID
 */
void HIDReceiverApp::initHIDPads(){
	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;
	
	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;	
	
	padsFoundCount = 0;
	while (cur_dev) {
		/*
		console() << "VendorId: " << cur_dev->vendor_id << " productId: " << cur_dev->product_id << " @ " << cur_dev->path << " = " ;
		printf("%ls\n" , cur_dev->product_string);
		*/
		
		if( cur_dev->vendor_id == PAD_VENDOR_ID && cur_dev->product_id == PAD_PRODUCT_ID ){
			console() << "Found PAD! @ " << cur_dev->path <<  std::endl;
			if( padsFoundCount < PAD_COUNT ){
				hid_device *handle = hid_open_path(cur_dev->path);//hid_open(cur_dev->vendor_id, cur_dev->product_id);
				hid_set_nonblocking(handle, 1);
				
				pads[padsFoundCount] = handle;	
				padsFoundCount++;
			}else{
				break;
			}
		}
		/*
		printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
			   cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
		printf("\n");
		printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
		printf("  Product:      %ls\n", cur_dev->product_string);
		printf("\n");
		*/
		
		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);
	
	console() <<  "Found " << padsFoundCount << " NES USB pads" << std::endl;
}

/**
 * List all devices. Taken from HIDAPI test example
 * @see https://github.com/signal11/hidapi/blob/master/hidtest/hidtest.cpp
 */
void HIDReceiverApp::__hidDEBUG(){
	int res;
	unsigned char buf[65];
#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	hid_device *handle;
	int i;
	
	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;
	
	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;	
	while (cur_dev) {
		printf("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
			   cur_dev->vendor_id, cur_dev->product_id, cur_dev->path, cur_dev->serial_number);
		printf("\n");
		printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
		printf("  Product:      %ls\n", cur_dev->product_string);
		printf("\n");
		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);
	
	
	// Set up the command buffer.
	memset(buf,0x00,sizeof(buf));
	buf[0] = 0x01;
	buf[1] = 0x81;
	
	
	// Open the device using the VID, PID,
	// and optionally the Serial number.
	////handle = hid_open(0x4d8, 0x3f, L"12345");
	handle = hid_open(PAD_VENDOR_ID, PAD_PRODUCT_ID, NULL);
	if (!handle) {
		printf("unable to open device\n");
		return;
	}
	
	// Read the Manufacturer String
	wstr[0] = 0x0000;
	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read manufacturer string\n");
	printf("Manufacturer String: %ls\n", wstr);
	
	// Read the Product String
	wstr[0] = 0x0000;
	res = hid_get_product_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read product string\n");
	printf("Product String: %ls\n", wstr);
	
	// Read the Serial Number String
	wstr[0] = 0x0000;
	res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read serial number string\n");
	printf("Serial Number String: (%d) %ls", wstr[0], wstr);
	printf("\n");
	
	// Read Indexed String 1
	wstr[0] = 0x0000;
	res = hid_get_indexed_string(handle, 1, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read indexed string 1\n");
	printf("Indexed String 1: %ls\n", wstr);
	
	// Set the hid_read() function to be non-blocking.
	hid_set_nonblocking(handle, 1);
	
	// Try to read from the device. There shoud be no
	// data here, but execution should not block.
	res = hid_read(handle, buf, 17);
	
	// Send a Feature Report to the device
	buf[0] = 0x2;
	buf[1] = 0xa0;
	buf[2] = 0x0a;
	buf[3] = 0x00;
	buf[4] = 0x00;
	res = hid_send_feature_report(handle, buf, 17);
	if (res < 0) {
		printf("Unable to send a feature report.\n");
	}
	
	memset(buf,0,sizeof(buf));
	
	// Read a Feature Report from the device
	buf[0] = 0x2;
	res = hid_get_feature_report(handle, buf, sizeof(buf));
	if (res < 0) {
		printf("Unable to get a feature report.\n");
		printf("%ls", hid_error(handle));
	}
	else {
		// Print out the returned buffer.
		printf("Feature Report\n ");
		for (i = 0; i < res; i++)
			printf("%02hhx ", buf[i]);
		printf("\n");
	}
	
	memset(buf,0,sizeof(buf));
	
	// Toggle LED (cmd 0x80). The first byte is the report number (0x1).
	buf[0] = 0x1;
	buf[1] = 0x80;
	res = hid_write(handle, buf, 17);
	if (res < 0) {
		printf("Unable to write()\n");
		printf("Error: %ls\n", hid_error(handle));
	}
	
	
	// Request state (cmd 0x81). The first byte is the report number (0x1).
	buf[0] = 0x1;
	buf[1] = 0x81;
	hid_write(handle, buf, 17);
	if (res < 0)
		printf("Unable to write() (2)\n");
	
	// Read requested state. hid_read() has been set to be
	// non-blocking by the call to hid_set_nonblocking() above.
	// This loop demonstrates the non-blocking nature of hid_read().
	res = 0;
	while (res == 0) {
		res = hid_read(handle, buf, sizeof(buf));
		if (res == 0)
			printf("waiting...\n");
		if (res < 0)
			printf("Unable to read()\n");
#ifdef WIN32
		Sleep(500);
#else
		usleep(500*1000);
#endif
	}
	
	printf("Data read:\n ");
	// Print out the returned buffer.
	for (i = 0; i < res; i++)
		printf("%02hhx ", buf[i]);
	printf("\n");
}

/** 
 * Converts a integer into binary string and prints it into stdout
 * @see http://www.daniweb.com/software-development/cpp/threads/12304/1242679#post1242679
 */
void HIDReceiverApp::int_bin(int f){
	int x = *(int *)&f;
    std::bitset<sizeof(int) * 8> binary(x);
    std::cout << binary;    
}

CINDER_APP_BASIC( HIDReceiverApp, RendererGl )
