//***************************************************
//* Library to simplify working with DWIN Displays  *
//* Lib use FreeRTOS, so for ESP32 only             *
//* Copyright (C) 2024 Pavel Pervushkin.  Ver.1.0.2 *
//* Released under the MIT license.                 *
//***************************************************


#ifndef Dwin2_h
#define Dwin2_h

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "vector"
#include <codecvt>
#include <locale>
#include <HardwareSerial.h>


#define BUFSIZE 256
#define HW_SERIAL_NUM 2

typedef enum {
    INT,
    DOUBLE,
    UTF,
    ASCII,
    ICON
} uitype_t;

typedef struct {
    size_t size;
    uint8_t *cmd;
} cmdtype_t;

typedef enum {
    RED,
    BLUE,
    GREEN,
    ORANGE,
    PURPLE,
    TURQUOISE,
    BROWN,
    PINK,
    DARK_GREEN,
    YELLOW_GREEN,
    ROSE_RED,
    DEEP_PURPLE,
    SKY_BLUE,
    GRAY,
    BLACK,
    DARK_BLUE,
    WHITE
} uicolor_t;


//***********************************************************************************************************************
//************* DWIN2 main class ****************************************************************************************
//***********************************************************************************************************************
class DWIN2
{
private:
    // Increment/decrement text or numeric integer data by delta
    void increment(const double &delta);

    // Send a command to read the numeric value of the UI element
    void sendReadUiNumCmd();
    // Send a command to read the text value of the UI element
    // The larger the maxTextSize value, the longer the buffer, so it is not recommended to
    // set the value large, if the length of the intended text does not require it
    void sendReadUiTextCmd(const uint8_t &maxTextSize);

    // Response processing after sending sendReadUiNumCmd() or sendReadUiTextCmd() commands
    int hexBufIntProcessing(const std::vector<uint8_t> &buffer);
    double hexBufDblProcessing(const std::vector<uint8_t> &buffer);
    String hexBufUtfProcessing(const std::vector<uint8_t> &buffer);
    String hexBufAsciiProcessing(const std::vector<uint8_t> &buffer);

    // Send command to send over UART, with mutex
    void sendUart(const uint8_t *command, const uint8_t &cmdLength);

    static String _dwinEcho;   // Storing the response from the display

    // Data from the response of the sent command
    String _uiData = "";
    bool _echo; // Listen to the response from the display

    // Handle blinking of UI elements
    esp_timer_handle_t _blinkTimerHandle;
    static void blinkTmr(void *arg);
    uint64_t _blinkPeriod;

    // Blink status
    static bool _isBlink;
    void blinkUI(bool state);

    // Communication with the display via uart
    static HardwareSerial *_uart;

    uint16_t _spHexAddr;
    uint16_t _vpHexAddr;
    uitype_t _uitype;
    uint8_t _id;

    // Buffer for storing the command
    static std::vector<uint8_t> _uartCmdBuffer;  
 
    // Read UART
    static std::vector<uint8_t> _uartRxBuf;
    SemaphoreHandle_t _uartUiReadSem; // Binary semaphore

    // Mutex for UART
    static SemaphoreHandle_t _uartMutex;
    // Mutex for buffer
    static SemaphoreHandle_t _bufferMutex;

    // Limits and delta
    int _minVal;
    int _maxVal;
    double _delta;
    // Current value for UI element
    double _currentVal;
    // Storing a text array for the UI element
    std::vector<String> _listStrVal;
    // Rotation direction (for Encoder knob)
    bool _rightDir;
    bool _loopRotation;

    void swapBytes(uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size / 2; ++i) {
            uint8_t temp = bytes[i];
            bytes[i] = bytes[size - i - 1];
            bytes[size - i - 1] = temp;
        }
    }

    String utf16_to_utf8(const uint16_t* utf16, size_t utf16_len);

    // Output array in HEX format
    template<typename Type>
    static String printHex(Type c, int arrSize)
    {
        static const char hex_digits[] = "0123456789ABCDEF";
        uint8_t hexCharArr[arrSize] = {};
        String hexStr;
        hexStr.reserve(arrSize*4);
        for (int i=0; i < arrSize; i++)
        {
            hexStr.concat(hex_digits[c[i] >> 4]);
            hexStr.concat(hex_digits[c[i] & 15]);
            hexStr.concat(" ");
        }
        return hexStr;
    };

protected:
    typedef std::function<void(DWIN2 &uartcb)> CallbackFunction;
    CallbackFunction uartEcho_cb = NULL;
    void _handleEchoUart();

    // Clearing the display buffer DWIN
    void clearRxBuf();

    // Processing the response from sent commands to DWIN Display
    static void uartTask(void* parameter); // Static method to be run in the thread
    TaskHandle_t _taskHandleUart = nullptr; // FreeRTOS task descriptor
    SemaphoreHandle_t _uartWriteSem; // Bynary counting semaphore

public:
    DWIN2();
    ~DWIN2();

    void begin(const uint16_t &spHexAddr = 0, const uint16_t &vpHexAddr = 0, const uint8_t &rxPin = 16, const uint8_t &txPin = 17);

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
    // Set Variables Icon
    void setVarIcon(const int &icoNum);
    // Set UI-element position
    void setPos(const int &x, const int &y);
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
    // 
    uint8_t getVarIconIndex();
};



#endif