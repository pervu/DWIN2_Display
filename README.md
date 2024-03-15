# DWIN2_Display

Advanced ESP32 Libabry for DWIN DGUS T5L HMI Displays.<br>
It is unofficial library for the DWIN DGUS Displays whith extended features for ESP32.<br>
Does not block execution of other code. Sends and receives data as fast as possible.<br>

## DWIN2 Class Methods
```cpp
    // Common methods
    // Set page number
    void setPage(const uint8_t &pageNum);
    // Get page number
    uint8_t getPage();
    // Set diaplay brightness
    void setBrightness(const uint8_t &brightness);
    // Get diaplay brightness
    uint8_t getBrightness();
    // Restart display
    void restartHMI();

    // Methods for ui elements
    // Set the id of the object to be created
    void setId(const uint8_t &id);
    // Setting a new address for the UI element
    void setAddress(const uint16_t &spHexAddr, const uint16_t &vpHexAddr);
    // Select the UI type of the display element
    void setUiType(const uitype_t &uitype);
    // Set the min. and max. values, 
    // delta to increase/decrease the value
    void setLimits(const uint32_t &minVal, const uint32_t &maxVal, const bool &loopRotation = false);
    // For text data, limits can be calculated automatically
    void setLimits(const bool& loopRotation = true);
    // Setting the initial value
    void setStartVal(const double &currentVal);
    // Set a text list of values for UI elements
    void setStrListVal(const std::vector<String> listStrVal);
    // Set blink rate in milliseconds
    void setBlinkPeriod(const uint64_t &blinkPeriodMs);
    // Setting the called colbeck function
    void setUartCbHandler(CallbackFunction f);
    // Enable/disable echo mode
    void setEcho(const bool &echo);
    // Set color
    // Overloaded function
    void setColor(uint16_t colorHex);
    void setColor(uicolor_t color);
    // Dwin answer
    String getDwinEcho();
    // Blink ui element.
    void blink(const bool &isBlink);
    // Hide/unhide UI element
    void showUi();
    void hideUi();
    // Sending numeric/text values to the display
    // Overloaded function
    void sendData(const int &data);
    void sendData(const double &data);
    void sendData(const String &data);
    // Send the command to the display in Hex format
    void sendRawCommand(const uint8_t *cmd, const size_t &cmdLength);
    // Increment/decrement the value by a specified delta depending on the direction when calling the method
    void update(const double &delta = 1.0, const bool &rightDir = true);
    // Clearing the text field
    void clearText(uint8_t length = 10);
    // Get blink status
    bool getBlinkStatus();
    // Get the current value (as a number for int and dbl values and as an index for text values)
    double getCurrentVal();
    // Get object ID
    uint8_t getId();
    // Read data from UI element
    String getUiData(const uint8_t &textSize = 10);
```

## AS5600 QuickStart
```cpp
#include <Arduino.h>
#include <Dwin2.h>

// Rx Tx ESP gpio connected to DWin Display
#define RX_PIN 16
#define TX_PIN 17

// Class of controlling UI elements of the display
DWIN2 dwc;

// Callback function to receive a response from the display
void dwinEchoCallback(DWIN2 &d);

void setup() {
    Serial.begin(115200);
    while (Serial.available()) {}
    Serial.printf("-------- Start DWIN communication demo --------\n");
    Serial.printf("-----------------------------------------------\n");

    const int d = 1000;
    delay(d);

//----------------------------------------------------------------------------------------

    Serial.printf("\n----- Dwin Display Common commands start -----\n");
    //************ DWIN Display Common commands **************
    // Init timers, tasks, serial communication and other
    dwc.begin();
    // Set callback for answer
    dwc.setUartCbHandler(dwinEchoCallback);
    // Show commands and answers
    dwc.setEcho(true);

//----------------------------------------------------------------------------------------  

    // General commands that refer to the display and not its UI elements
    // Set/get display pages
    // Set page
    dwc.setPage(2);
    delay(d);
    dwc.setPage(1);
    delay(d);
    dwc.setPage(0);
    delay(d);
    // Get page
    uint8_t pageNum = dwc.getPage();
    Serial.printf("Current page: %d\n", pageNum);
    delay(d);

    // Set/get brightness
    // Set display brightness (Should be from 0 to 127)
    dwc.setBrightness(15);
    delay(d);
    uint8_t brtn = 0;
    // Get display current brightness
    brtn = dwc.getBrightness();
    Serial.printf("Current brightness: %d\n", brtn);
    delay(d);
    // Set display brightness
    dwc.setBrightness(127);
    delay(d);
    // Get display current brightness
    brtn = dwc.getBrightness();
    Serial.printf("Current brightness: %d\n", brtn);
    delay(d);

    // Send raw command
    // Set display brightness to 0%
    const uint8_t rawCmd1[] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x82, 0x00};
    // Set display brightness to 100%       
    const uint8_t rawCmd2[] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x82, 0x7F};       
    dwc.sendRawCommand(rawCmd1, sizeof(rawCmd1));
    delay(d);
    dwc.sendRawCommand(rawCmd2, sizeof(rawCmd2));
    delay(d);

//----------------------------------------------------------------------------------------

    // Restart HMI
    //dwc.restartHMI();
    delay(1000);
    Serial.printf("----- Dwin Display common commands end -----\n");

//----------------------------------------------------------------------------------------

    Serial.printf("\n----- Dwin UI commands start -----\n");
    // Set ui adress
    dwc.setAddress(0x9000, 0x1000);
    // Set ui type of the display elemet communicating with
    dwc.setUiType(INT);
    // Set text color
    dwc.setColor(SKY_BLUE);
    delay(d);
    // Or set color in DWIN HEX fromat
    dwc.setColor(0xFFFF);
    // Send data
    dwc.sendData(55);
    // Set start val. Same as sendData. It is used with update() method. 
    dwc.setStartVal(20);
    // Set limits for the ui-elements. It will not less then min value, and greater than max value
    // You also can turn on cycle rotation when value reaches min or max value
    dwc.setLimits(10, 50, true);
    for (int i = 0; i < 10; i++)
    {
        // Increment/decrement the value by a given delta depending on the direction
        dwc.update(2.0, true);
        delay(200);
    }
    // Get value of the ui-element always return String
    String val = dwc.getUiData();
    Serial.printf("UI value: %s\n", val);
    // Hide ui element
    dwc.hideUi();
    delay(d);
    // Show ui element
    dwc.showUi();
    delay(d);
    // Change ui address
    dwc.setAddress(0x9030, 0x1030);
    // Set new UI type
    dwc.setUiType(UTF);      // Set type of the UI element of the dwin display
    // Send UTF text to display
    dwc.sendData("UTF Текст");

//----------------------------------------------------------------------------------------
    // Change ui address, return it to INT ui
    dwc.setAddress(0x9030, 0x1030);
    // Set new UI type
    dwc.setUiType(INT);      // Set type of the UI element of the dwin display
    // Lets blink it!
    // Blinking EXAMPLES:
    // Set blink period
    Serial.printf("Blink 200\n");
    dwc.setBlinkPeriod(200);
    // Start blink
    dwc.blink(true);
    delay(2000);
    // Set other blink period
    dwc.setBlinkPeriod(600);
    Serial.printf("Blink 800\n");
    delay(3000);
    // Stop blinking
    dwc.blink(false);
    delay(d);

//----------------------------------------------------------------------------------------

    // update() method is good to use with some interrupts,
    // such as button pushes, encoder rotating, etc.
    // update() EXAMPLES:
    // Set ui adress for the ASCII element
    dwc.setAddress(0x9020, 0x1020);
    // Set ui type of the display elemet communicating with
    dwc.setUiType(ASCII);
    std::vector<String> asciiList = {"One", "Two", "Three", "Four", "Five"};
    dwc.setStrListVal(asciiList);
    dwc.setLimits();
    // Then set start value as index of the list
    dwc.setStartVal(2);
    // Simulate encoder rotation
    for (int i = 0; i < 10; i++)
    {
        dwc.update(true);
        delay(100);
    }

    // Set ui adress for the double element
    dwc.setAddress(0x9010, 0x1010);
    // Set ui type of the display elemet communicating with
    dwc.setUiType(DOUBLE);
    // Send some int data
    dwc.setStartVal(25.8);
    dwc.setLimits(10, 50, true);
    for (int i = 0; i < 20; i++)
    {
        dwc.update(0.1, true);
        delay(100);
    }
    delay(d);
    Serial.printf("------ Dwin UI commands examples end  ------\n");

//----------------------------------------------------------------------------------------

    Serial.printf("\n--------------------------------------------------\n");
    Serial.printf("-------- DWIN communication demo finished --------\n");
}


void loop() {
    delay(portMAX_DELAY);
}

void dwinEchoCallback(DWIN2 &d)
{
    Serial.print("Echo ");
    Serial.println(d.getDwinEcho());
}
```
