#include "NinjaBlockEthernet.h"

//#include <MemoryFree.h> 

byte mac[] = { 0xCE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

Adafruit_CC3000_Client client;

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define ALLNOTNULL(A, B, C) ((A!=NULL) && (B!=NULL) && (C!=NULL))

#define WLAN_SSID       "myNetwork"           // cannot be longer than 32 characters!
#define WLAN_PASS       "myPassword"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

int NinjaBlockClass::begin()
{   uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
	int result = 1;
	if (ALLNOTNULL(host, nodeID, token) // has connection params
		&& cc3000.begin()// has Dynamic IP address
		)
	{
		result = 1;
		Serial.print("IP: ");
		cc3000.printIPdotsRev(ipAddress);
		Serial.println();
	}
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
    Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100);
  }  
	return result;
}

void NinjaBlockClass::httppost(char *postData)
{	uint32_t ip;
	Serial.print("_");
	int length;
    if (!client.connected()) {
	      while (ip == 0) {
			if (! cc3000.getHostByName(host, &ip)) {
			Serial.println(F("Couldn't resolve!"));
			}
			delay(500);
		  }
        client = cc3000.connectTCP(ip,port);
    }
	if (client.connected()) {
		sendHeaders(true,client);
		client.fastrprint("Content-Length: ");
		length = strlen(postData);
		client.println(length);
		client.println();
		client.fastrprintln(postData);	
		Serial.print("Sent=");
		Serial.println(postData);		
		client.close();
	} else {
		Serial.println("Send Failed");
	}
	return;
}

void NinjaBlockClass::sendHeaders(bool isPOST, Adafruit_CC3000_Client hclient) {
	char strData[DATA_LEN];
	if (isPOST)  
		strcpy(strData,"POST");
	else 
		strcpy(strData,"GET");
	strcat(strData," /rest/v0/block/");
	strcat(strData, nodeID);
	if (isPOST)  
		strcat(strData,"/data");
	else 
		strcat(strData, "/commands");
	strcat(strData, " HTTP/1.1\r\n");
	hclient.fastrprint(strData);
	strcpy(strData,"Host: "); 
	strcat(strData ,host);
	strcat(strData, "\r\n");
	hclient.fastrprint(strData);
	hclient.fastrprint("User-Agent: Ninja Arduino 1.1\r\n\
Content-Type: application/json\r\n\
Accept: application/json\r\n");
	strcpy(strData,"X-Ninja-Token: ");
	strcat(strData, token);
	strcat(strData,"\r\n");
	hclient.fastrprint(strData);
}

void NinjaBlockClass::send(char *data)
{
	ninjaMessage(false, 0, data);
}

void NinjaBlockClass::send(int data)
{
	ninjaMessage(true, data, 0);
}

//Most Arduinos, 2-byte int range -32,768 to 32,767 (max 6 chars)
//Arduino Duo, 4-byte range -2,147,483,648 to 2,147,483,647 (max 11 chars)
const char kNumLength = sizeof(int) * 3;
char strNumber[kNumLength];
//return reference to strNumber
char *int2str(int num) {
	return itoa(num, strNumber, 10); // base 10
}
char strSend[DATA_SIZE];
void addStringAndUnderscore(char * str) {
	strcat(strSend, str);
	strcat(strSend, "_");
}

void NinjaBlockClass::ninjaMessage(bool isInt, int intData, char *charData) {
	if (guid != NULL) {
		strcpy(strSend,"{\"GUID\": \"");
		addStringAndUnderscore(nodeID);
		addStringAndUnderscore(guid);
		addStringAndUnderscore(int2str(vendorID));
		strcat(strSend, int2str(deviceID));
		strcat(strSend, "\",\"G\": \"");
		strcat(strSend, guid);
		strcat(strSend, "\",\"V\": ");
		strcat(strSend, int2str(vendorID));
		strcat(strSend,",\"D\": ");
		strcat(strSend, int2str(deviceID));
		strcat(strSend, ",\"DA\": ");
		if (isInt) {
			strcat(strSend, int2str(intData));
		} else {
			strcat(strSend, "\"");
			strcat(strSend, charData);
			strcat(strSend, "\"");
		}
		strcat(strSend, "}");
		httppost(strSend);
	}
}


const char kStrHeaderEnd[] = {'\r', '\n', '\r', '\n'};
const byte kHeaderLength = sizeof(kStrHeaderEnd);

// will keep reading bytes until it has matched the header, or it has read all available bytes
inline void skipHeader(const int bytesAvailable, int &bytesRead) {
	//skip past header
	for (uint8_t matching=0
		; (matching < kHeaderLength) && (bytesRead < bytesAvailable)
		; bytesRead++) {
		matching = ((recvclient.read() == kStrHeaderEnd[matching]) ? matching+1 : 0);
	}
}

const char kCharInvertedCommas	= '\"';
bool NinjaBlockClass::receive(void) {
	bool gotData = false;
	if(!recvclient.connected())
	{
		// connect if not connected
		Serial.print(".");
		recvclient.stop();
		if(recvclient.connect(host,port)==1)
		{
			sendHeaders(false, recvclient);
			recvclient.println();
		}
	}
	if (recvclient.connected())
	{
		gotData = receiveConnected();
	}
	return gotData;
}

// giving a name prefix, eg. -> G":" <-, skip past the value after it, and insert a string terminator
// returns NULL, or the beginning of a string within data that has been null-terminated
char * valueString(const char *name, char *data, int &index, const int length) {
	char *result = NULL;
	uint8_t nameLength = strlen(name);
	for (uint8_t matching=0
		; (matching < nameLength) && (index < length)
		; index++) {
		matching = ((data[index] == name[matching]) ? matching+1 : 0);
	}
	if (index < length) {
		//if searching for a string seek end of string ("), otherwise (,) ends an int
		char endChar = (data[index-1]==kCharInvertedCommas) ? kCharInvertedCommas : ',';
		int start = index;
		while ((index < length) && (data[index] != endChar)) {
			index++;
		}
		if (index < length) {
			data[index] = '\0'; // insert string terminator after value (string or int)
			result = &data[start];
		}
	}
	return result;
}

bool NinjaBlockClass::receiveConnected(void) {
	bool gotHeader = false;
	bool gotData = false;
	int bytesAvailable = recvclient.available();
	if (bytesAvailable > 0)
	{
		int bytesRead = 0;
		skipHeader(bytesAvailable, bytesRead);
		gotHeader = (bytesRead < bytesAvailable); //skipped header without reaching end of available bytes
		if (gotHeader) {
			//reset counts
			bytesAvailable -= bytesRead;
			bytesRead = 0;

			if (bytesAvailable > DATA_SIZE) {
				Serial.print("ERR: DATA_SIZE");
				return false;
			}

			char data[DATA_SIZE];
			//read data into array eg. {"DEVICE":[{"G":"0","V":0,"D":1000,"DA":"FFFFFF"}]}
			for (bytesRead=0; bytesRead<bytesAvailable; bytesRead++) {
				data[bytesRead] = recvclient.read();
				//Serial.print(data[bytesRead]);
			}
			data[bytesRead] = '\0'; //terminate data as string
			bytesRead = 0;
			char *strVal;
			strVal = valueString("G\":\"", data, bytesRead, bytesAvailable);
			if (strVal) {
				strcpy(strGUID, strVal);
				strVal = valueString("V\":", data, bytesRead, bytesAvailable);
				if (strVal != NULL) {
					intVID = atoi(strVal);
					strVal = valueString("D\":", data, bytesRead, bytesAvailable);
					if (strVal != NULL) {
						intDID = atoi(strVal);

					 	// Serial.print(" strGUID=");
					 	// Serial.println(strGUID);
					 	// Serial.print(" intVID=");
					 	// Serial.println(intVID);
					 	// Serial.print(" intDID=");
					 	// Serial.println(intDID);

						int start = bytesRead;
						strVal = valueString("DA\":\"", data, bytesRead, bytesAvailable);
						if (strVal != NULL) {
							strcpy(strDATA, strVal);
							IsDATAString = true;
							gotData = true;
							// Serial.print("strDATA=");
							// Serial.println(strDATA);
						}
						else { // may be an int value
							bytesRead = start; // reset to where we were before attempting (data is unmodified if NULL was returned)
							strVal = valueString("DA\":", data, bytesRead, bytesAvailable);
							if (strVal) {
								intDATA = atoi(strVal);
								IsDATAString = false;
								gotData = true;
								// Serial.print("intDATA=");
								// Serial.println(intDATA);
							}
						}
					}
				}
			}
		}
	}
	if (gotHeader) {
		//if a header was received, there was some data after (either json, or some html etc)
		//purge and close the stream
		recvclient.flush();
		delay(100);
		recvclient.stop();
		delay(100);
	}
	return gotData;
}

NinjaBlockClass NinjaBlock;
