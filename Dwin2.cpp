#include <Dwin2.h>

// Define static variables
HardwareSerial* DWIN2::_uart = nullptr;
SemaphoreHandle_t DWIN2::_uartMutex;
SemaphoreHandle_t DWIN2::_bufferMutex;
bool DWIN2::_isBlink = false;
std::vector<uint8_t> DWIN2::_uartCmdBuffer;
std::vector<uint8_t> DWIN2::_uartRxBuf;
String DWIN2::_dwinEcho;

//***********************************************************************************************************************
//************* DWIN2 main class ****************************************************************************************
//***********************************************************************************************************************
DWIN2::DWIN2()
{
    _isBlink = false;
    _blinkPeriod = 500000;
    _dwinEcho.reserve(BUFSIZE);
    _uartCmdBuffer.reserve(BUFSIZE); 
    _uartRxBuf.reserve(BUFSIZE);
}

DWIN2::~DWIN2()
{
    if (_taskHandleUart)
    {
        vTaskDelete(_taskHandleUart);
    }
}

void DWIN2::begin(const uint16_t &spHexAddr, const uint16_t &vpHexAddr, const uint8_t &rxPin, const uint8_t &txPin)
{
    _spHexAddr = spHexAddr;
    _vpHexAddr = vpHexAddr;

    // UART initialization
    _uart = new HardwareSerial(HW_SERIAL_NUM);
    if (_uart) _uart->begin(115200, SERIAL_8N1, rxPin, txPin);

    // Task for listening to uart
    // Create semaphore for the message about sending command via UART
    _uartWriteSem = xSemaphoreCreateCounting(4, 0);

    // Creating mutexes
    _uartMutex = xSemaphoreCreateMutex();
    _bufferMutex = xSemaphoreCreateMutex();

    // Semaphore for control of receiving data into the _uartBuf array
    _uartUiReadSem = xSemaphoreCreateCounting(1, 0);

    // Create task for UART
    xTaskCreatePinnedToCore(
        this->uartTask,          
        "DwinUartTask",          
        2048,                      
        this,                     
        1,                  
        &_taskHandleUart,      
        0                 
    );

    // Timer configuration for blinking
    esp_timer_create_args_t timerConfig;
    timerConfig.arg = this;
    timerConfig.callback = reinterpret_cast<esp_timer_cb_t>(blinkTmr);
    timerConfig.dispatch_method = ESP_TIMER_TASK;
    timerConfig.name = "encTimer";
    esp_timer_create(&timerConfig, &_blinkTimerHandle);
}

void DWIN2::setId(const uint8_t &id)
{
    _id = id;
}

void DWIN2::setAddress(const uint16_t &spHexAddr, const uint16_t &vpHexAddr)
{
    _spHexAddr = spHexAddr;
    _vpHexAddr = vpHexAddr;
}

void DWIN2::setUiType(const uitype_t &uitype)
{
    _uitype = uitype;
}

void DWIN2::setLimits(const uint32_t &minVal, const uint32_t &maxVal, const bool &loopRotation)
{
    _minVal = minVal;
    _maxVal = maxVal;
    _loopRotation = loopRotation;
}

void DWIN2::setLimits(const bool &loopRotation)
{
    if (_listStrVal.size() > 0)
    {
        _minVal = 0;
        _maxVal = _listStrVal.size()-1;
        _loopRotation = loopRotation;
    }
    else{
        Serial.printf("ID%d ERR setLimits() Текстовый список пуст. Лимиты не установлены!\n", _id);
    }
}

void DWIN2::setStartVal(const double &currentVal)
{
    _currentVal = currentVal;
    
    int currIntVal = static_cast<int>(_currentVal);
    switch (_uitype)
    {
    case INT:
        sendData(currIntVal);
        break;
    case DOUBLE:
        sendData(_currentVal);
        break;
    case ASCII:
        if (_listStrVal.size() > currIntVal)
        {
            sendData(_listStrVal.at(currIntVal).c_str());
        }
        else
        {
            Serial.printf("ID %d ERR setStartVal() ASCII _listStrVal not filled or less than _currentVal.\n", _id);
        }
        break;
    case UTF:
        if (_listStrVal.size() > currIntVal)
        {
            sendData(_listStrVal.at(currIntVal).c_str());
        }
        else
        {
            Serial.printf("ID %d ERR setStartVal() UTF _listStrVal not filled or less than _currentVal.\n", _id);
        }
        break;
    
    default:
        Serial.printf("ID %d ERR setStartVal() unknown UI type.\n", _id);
        break;
    }
    
}

void DWIN2::setStrListVal(const std::vector<String> listStrVal)
{
    _listStrVal.assign(listStrVal.begin(), listStrVal.end());
}

void DWIN2::setBlinkPeriod(const uint64_t &blinkPeriodMs)
{
    _blinkPeriod = blinkPeriodMs*1000;
    // Restart timer
    // Check if timer already running
    if (esp_timer_is_active(_blinkTimerHandle))
    {
        esp_timer_stop(_blinkTimerHandle);
    }
    // Start timer
    esp_timer_start_periodic(_blinkTimerHandle, _blinkPeriod);
}

void DWIN2::setUartCbHandler(CallbackFunction f)
{
    uartEcho_cb = f;
}

void DWIN2::setEcho(const bool &echo)
{
    _echo = echo;
}

void DWIN2::setColor(uint16_t colorHex)
{
    const uint8_t commandLen = 8;
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    uint16_t spColorAddr = _spHexAddr + 0x03;   // SP is offset by 3 bits
    command[4] = highByte(spColorAddr);
    command[5] = lowByte(spColorAddr);
    command[6] = highByte(colorHex);
    command[7] = lowByte(colorHex);
    
    // Send data to uartTask
    sendUart(command, commandLen);
}

void DWIN2::setColor(uicolor_t color)
{
    uint16_t colorHex;
    switch (color)
    {
    case 0:
        colorHex = 0xF800;  // Red: 0xF800
        break;
    case 1:                 // Blue: 0x001F
        colorHex = 0x001F;
        break;
    case 2:                 // Green: 0x07E0
        colorHex = 0x07E0;
        break;
    case 3:                 // Orange: 0xFC00
        colorHex = 0xFC00;
        break;
    case 4:                 // Purple: 0x801F
        colorHex = 0x801F;
        break;
    case 5:                 // Turquoise: 0x07FF
        colorHex = 0x07FF;
        break;
    case 6:                 // Brown: 0x4000
        colorHex = 0x4000;
        break;
    case 7:                 // Pink: 0xFC1F
        colorHex = 0xFC1F;
        break;
    case 8:                 // Dark green: 0x0208
        colorHex = 0x0208;
        break;
    case 9:                 // Yellow-green: 0x8400
        colorHex = 0x8400;
        break;
    case 10:                // Rose red: 0xF810
        colorHex = 0xF810;
        break;
    case 11:                // Deep Purple: 0x4010
        colorHex = 0x4010;
        break;
    case 12:                 // Sky blue: 0x041F
        colorHex = 0x041F;
        break;
    case 13:                 // Neutral gray: 0x8410
        colorHex = 0x8410;
        break;
    case 14:                // Black: 0x0000
        colorHex = 0x0000;
        break;
    case 15:                // Dark blue: 0x0010
        colorHex = 0x0010;
        break;
    case 16:                // White: 0xFFFF
        colorHex = 0xFFFF;
        break;
    
    default:
        break;
    }

    const uint8_t commandLen = 8;
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    uint16_t spColorAddr = _spHexAddr + 0x03;   // SP is offset by 3 bits
    command[4] = highByte(spColorAddr);
    command[5] = lowByte(spColorAddr);
    command[6] = highByte(colorHex);
    command[7] = lowByte(colorHex);
    
    // Send data to uartTask
    sendUart(command, commandLen);
}

String DWIN2::getDwinEcho()
{
    return _dwinEcho;
}

void DWIN2::blink(const bool &isBlink)
{
    _isBlink = isBlink;
    if (_isBlink)
    {
        if (esp_timer_is_active(_blinkTimerHandle))
        {
            esp_timer_stop(_blinkTimerHandle);
        }
        esp_timer_start_periodic(_blinkTimerHandle, _blinkPeriod);
    }
    else
    {
        // clear rx buffer
        clearRxBuf();
        if (esp_timer_is_active(_blinkTimerHandle))
        {
            esp_timer_stop(_blinkTimerHandle);
            // Enable display, in case the UI element was hidden
            if (!_isBlink) showUi();
        }
    }
}

void DWIN2::sendUart(const uint8_t * command, const uint8_t &cmdLength)
{
    // Block access to the buffer during copying
    if (xSemaphoreTake(_bufferMutex, portMAX_DELAY) == pdTRUE)
    {
        // Copy data from command to buffer
        for (int i = 0; i < cmdLength; i++)
        {
            _uartCmdBuffer.push_back(command[i]);
        }
        // Send the command to uartTask
        xSemaphoreGive(_uartWriteSem);
        // Giving access
        xSemaphoreGive(_bufferMutex);
    }
}

void DWIN2::sendData(const int &data)
{
    if (_uitype != INT) 
    {
        Serial.printf("ID%d ERR sendData() wrong ui type, should be INT\n", _id);
        return;
    }
    const uint8_t commandLen = 8;
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);
    command[6] = highByte(data);
    command[7] = lowByte(data);
    sendUart(command, commandLen);
}

void DWIN2::sendData(const double &data)
{
    if (_uitype != DOUBLE) 
    {
        Serial.printf("ID%d ERR sendData() wrong ui type, should be DOUBLE\n", _id);
        return;
    }
    const uint8_t headerLen = 6;
    const uint8_t commandLen = 14;

    uint8_t command[commandLen] = {0x5A, 0xA5, 0x0B, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);

    double dbl = data;
    uint8_t* byteArrDbl = reinterpret_cast<uint8_t*>(&dbl);
    // Swap bytes (big endian to little endian)
    swapBytes(byteArrDbl, sizeof(double));

    // Pointer to the 6th element of the array (end of the array command)
    uint8_t *pntr{&command[headerLen]};
    // Adding double after the command header
    memcpy(pntr, byteArrDbl, sizeof(double));
    
    sendUart(command, commandLen);
}

void DWIN2::sendData(const String &data)
{
    if (_uitype == ASCII)
    {
        const char* text = data.c_str();
        const unsigned char textLen = data.length();
        const unsigned char headerLen = 6;
        // Total array size is equal to header + text length + 2 bytes end of line
        const unsigned char commandLen = headerLen + textLen + 2;
        // The command length is counted without the first three bytes
        // Text length + 1 byte command (0x82) + 2 bytes _vpHexAddr + 2 bytes end of line
        const unsigned char cmdLen = data.length() + 5;
        // Define the hader
        unsigned char command[commandLen] = {0x5A, 0xA5, cmdLen, 0x82, 0x00, 0x00};
        command[4] = highByte(_vpHexAddr);
        command[5] = lowByte(_vpHexAddr);

        // Pointer to the 6th element of the array (end of the array command)
        unsigned char *pntr{&command[headerLen]};
        // Add text to the end of the array
        memcpy(pntr, text, textLen);
        // End-of-line character
        unsigned char eofl[2] = {0xFF, 0xFF};
        // Pointer to the penultimate element of the array
        unsigned char *pntr2{&command[commandLen-2]};
        // Add the end of the string to the end of the array
        memcpy(pntr2, eofl, sizeof(eofl));
        printHex(command, commandLen);

        // Send data to uartTask
        sendUart(command, commandLen);
    }
    else if (_uitype == UTF)
    {
        const char* text = data.c_str();
        // Convert ASCII text to UTF16
        // Create codecvt-object for conversion
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        // Convert ASCII string to UTF-16
        std::wstring utf16String = converter.from_bytes(text);
        // Convert std::wstring to unsigned char*
        const unsigned char* utf16text = reinterpret_cast<const unsigned char*>(utf16String.data());
        const unsigned char headerLen = 6;
        const unsigned char textLen = utf16String.length()*2;
        // Total array size is equal to header + text length + 2 bytes end of line
        const unsigned char commandLen = headerLen + textLen + 2;
        // The command length is counted without the first three bytes
        // Text length + 1 byte command (0x82) + 2 bytes _vpHexAddr + 2 bytes end of line
        const unsigned char cmdLen = textLen + 5;
        // Define the header
        unsigned char command[commandLen] = {0x5A, 0xA5, cmdLen, 0x82, 0x00, 0x00};
        command[4] = highByte(_vpHexAddr);
        command[5] = lowByte(_vpHexAddr);

        // Pointer to the 6th element of the array (end of the array command)
        unsigned char *pntr{&command[headerLen]};
        // Add text to the end of the buffer array
        memcpy(pntr, utf16text, textLen);
        // Change high and low bytes of text only
        for (int i = headerLen; i < commandLen; i += 2) 
        {
            std::swap(command[i], command[i+1]);
        }

        // End-of-line character
        unsigned char eofl[2] = {0xFF, 0xFF};
        // Pointer to the penultimate element of the array
        unsigned char *pntr2{&command[commandLen-2]};
        // Add the end of the string to the end of the array
        memcpy(pntr2, eofl, sizeof(eofl));

        // Send data to uartTask
        sendUart(command, commandLen);
    }
    else
    {
        Serial.printf("ID%d ERR sendData() wrong ui type, should be text\n", _id);
        return;
    }
}

void DWIN2::setVarIcon(const int &icoNum)
{
    if (_uitype != ICON) 
    {
        Serial.printf("ID%d ERR sendData() wrong ui type, should be ICON\n", _id);
        return;
    }
    const uint8_t commandLen = 8;
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);
    command[6] = highByte(icoNum);
    command[7] = lowByte(icoNum);
    sendUart(command, commandLen);
}


void DWIN2::setPos(const int &x, const int &y)
{
    const uint8_t commandLen = 10;
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint16_t offset = _spHexAddr + 1;
    command[4] = highByte(offset);
    command[5] = lowByte(offset);
    command[6] = highByte(x);
    command[7] = lowByte(x);
    command[8] = highByte(y);
    command[9] = lowByte(y);
    sendUart(command, commandLen);
}

void DWIN2::sendRawCommand(const uint8_t *cmd, const size_t &cmdLength)
{
    // Send data to uartTask
    sendUart(cmd, cmdLength);
}

void DWIN2::update(const double &delta, const bool &rightDir)
{
    _rightDir = rightDir;
    // Send incremental value to the display
    if ((_uitype == INT) || (_uitype == UTF) || ((_uitype == ASCII)) || (_uitype == DOUBLE))
    {
        increment(delta);
    }
    else
    {
        Serial.printf("ID%d, ERR update(), unknown UI type\n", _id);
    }
}

void DWIN2::clearText(uint8_t textLen)
{
    uint8_t commandLen = textLen*2 + 6;
    uint8_t cmdLen = commandLen-3;
    uint8_t command[commandLen] = {0x5A, 0xA5, cmdLen, 0x82, 0x00, 0x00};
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);

    if (_uitype == ASCII)
    {
        for (int i = 6; i < commandLen; i++)
        {
            command[i] = 0x20;
        }
        // Send data to uartTask
        sendUart(command, commandLen);
    }
    else if (_uitype == UTF)
    {
        for (int i = 6; i < commandLen; i++)
        {
            if (i%2==0) command[i] = 0x00;
            else command[i] = 0x20;
        }
        // Send data to uartTask
        sendUart(command, commandLen);
    }
    else
    {
        Serial.printf("ID%d: ERR clearText() Wrong ui type\n", _id);
    }
}

bool DWIN2::getBlinkStatus()
{
    return _isBlink;
}

double DWIN2::getCurrentVal()
{
    return _currentVal;
}


String DWIN2::getUiData(const uint8_t &textSize)
{
    String data;
    switch (_uitype)
    {
    case INT:
        sendReadUiNumCmd();
        while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
        data = (String)hexBufIntProcessing(_uartRxBuf);
        break;
    case DOUBLE:
        sendReadUiNumCmd();
        while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
        data = (String)hexBufDblProcessing(_uartRxBuf);
        break;
    case UTF:
        sendReadUiTextCmd(textSize);
        while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
        data = (String)hexBufUtfProcessing(_uartRxBuf);
        break;
    case ASCII:
        sendReadUiTextCmd(textSize);
        while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
        data = (String)hexBufAsciiProcessing(_uartRxBuf);
        break;
    default:
        data = "Unknown Data";
        break;
    }
    return data;
}

uint8_t DWIN2::getId()
{
    return _id;
}

void DWIN2::_handleEchoUart()
{
    if (uartEcho_cb != NULL) uartEcho_cb(*this);
}

void DWIN2::increment(const double &delta)
{
    if (_rightDir)
    {
        _currentVal = _currentVal + delta;
    }
    else{
        _currentVal = _currentVal - delta;
    }

    if (_loopRotation)
    {
        if (_currentVal < _minVal)
        {
            _currentVal = _maxVal;
        }
        else if (_currentVal > _maxVal)
        {
            _currentVal = _minVal;
        }
    }
    else
    {
        if (_currentVal < _minVal)
        {
            _currentVal = _minVal;
        }
        else if (_currentVal > _maxVal)
        {
            _currentVal = _maxVal;
        }
    }
    // Convert double to int
    int currIntVal = static_cast<int>(_currentVal);
    if (_uitype == INT)
    {
        sendData(currIntVal);
    }
    else if (_uitype == UTF)
    {
        if (_listStrVal.size() > currIntVal)
        {
            sendData(_listStrVal.at(currIntVal).c_str());
        }
        else
        {
            Serial.printf("ID %d ERR incrementInt() UTF _listStrVal не заполнен или меньше _currentVal.\n", _id);
            return;
        }
    }
    else if (_uitype == ASCII)
    {
        if (_listStrVal.size() > currIntVal)
        {
            sendData(_listStrVal.at(currIntVal).c_str());
        }
        else
        {
            Serial.printf("ID %d ERR incrementOnes() ASCII _listStrVal не заполнен или меньше _currentVal.\n", _id);
            return;
        }
    }
    else if (_uitype == DOUBLE)
    {
        sendData(_currentVal);
    }
}


void DWIN2::clearRxBuf()
{
    if (_uart->available() <= 0) return;
    if (xSemaphoreTake(_uartMutex, portMAX_DELAY) == pdTRUE)
    {
        _uart->flush();
        if (_echo) Serial.print("Clear rxBuf");
        while (_uart->available())
        {
            uint8_t buf = _uart->read();
            //if (_echo) Serial.print(buf, HEX);
        }
        if (_echo) Serial.println();
        xSemaphoreGive(_uartMutex);
    }
}

void DWIN2::sendReadUiNumCmd()
{
    const uint8_t commandLen = 7;
    std::vector<uint8_t> buffer;
    int num = 0;

    // Clear the receiving buffer of the display
    clearRxBuf();

    uint8_t command[commandLen] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x00, 0x00};
    if (_uitype == INT) command[6] = 0x01;
    else if (_uitype == DOUBLE) command[6] = 0x04;
    else Serial.printf("ID %d unknown _uitype\n");
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);

    // Отправка данных в задачу uartTask
    sendUart(command, commandLen);
}

void DWIN2::sendReadUiTextCmd(const uint8_t &maxTextSize)
{
    const uint8_t header = 8;
    const uint8_t commandLen = 7;
    // Clear buffer from previous messages
    clearRxBuf();
    // Адрес текстового поля
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x00, maxTextSize};
    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);

    // Send data to uartTask
    sendUart(command, commandLen);
}

int DWIN2::hexBufIntProcessing(const std::vector<uint8_t> &buffer)
{
    int num = 0;
    if ((buffer[3] == 0x83) && (buffer[4] == highByte(_vpHexAddr)) && (buffer[5] == lowByte(_vpHexAddr)))
    {
        num = static_cast<uint16_t>(buffer[7] << 8) | static_cast<uint16_t>(buffer[8]);
    }
    return num;
}

double DWIN2::hexBufDblProcessing(const std::vector<uint8_t> &buffer)
{
    double dnum = 0.0;
    std::vector<uint8_t> tmpBuf = buffer;
    if ((tmpBuf[3] == 0x83) && (tmpBuf[4] == highByte(_vpHexAddr)) && (tmpBuf[5] == lowByte(_vpHexAddr)))
    {
        // Reverse the array
        std::reverse(tmpBuf.begin(), tmpBuf.end());
        // Save the array to the variable double
        memcpy(&dnum, &tmpBuf[0], sizeof(double));                  
    }
    return dnum;
}

String DWIN2::hexBufUtfProcessing(const std::vector<uint8_t> &buffer)
{
    if ((buffer[3] == 0x83) && (buffer[4] == highByte(_vpHexAddr)) && (buffer[5] == lowByte(_vpHexAddr)))
    {
        int textBytesCounter = 0;
        unsigned char charText[buffer.size() - 6];

        // Select only the text part in UTF16 format
        for (auto it = buffer.begin(); it != buffer.end(); ++it)
        {
            int indx = it - buffer.begin();
            // End of text, the rest is garbage, exit.
            if ((*it == 0xFF) && (*(it+1) == 0xFF))
            {
                break;
            }
            if (indx > 6)
            {
                charText[textBytesCounter] = *it;
                textBytesCounter++;
            }
        }

        Serial.println(printHex(charText, textBytesCounter));

        // Collect the text part into words (2 bytes)
        uint8_t textU16Size = textBytesCounter/2;
        uint16_t utf16text[textU16Size];
        for (int i = 0; i < textU16Size; ++i)
        {
            utf16text[i] = static_cast<uint16_t>((charText[2*i] << 8) | charText[2*i+1]);
        }
        return utf16_to_utf8(utf16text, textU16Size);
    }
    return "";
}

String DWIN2::hexBufAsciiProcessing(const std::vector<uint8_t> &buffer)
{
    if ((buffer[3] == 0x83) && (buffer[4] == highByte(_vpHexAddr)) && (buffer[5] == lowByte(_vpHexAddr)))
    {
        String asciiStr = "";
        asciiStr.reserve(buffer.size() - 6);

        // Select only the text part in ASCII format
        for  (int i = 7; i < buffer.size()-1; i++)
        {   
            if ((buffer[i] == 0xFF) && (buffer[i+1] == 0xFF))
            {
                // End of text, the rest is garbage, exit.
                break;
            }
            asciiStr += static_cast<char>(buffer[i]);
        }
        return asciiStr;
    }
    return "";
}

void DWIN2::showUi()
{
    const uint8_t commandLen = 8;
    // Enable UI element display
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_spHexAddr);
    command[5] = lowByte(_spHexAddr);
    command[6] = highByte(_vpHexAddr);
    command[7] = lowByte(_vpHexAddr);
    // Send data to uartTask
    sendUart(command, commandLen);
}

void DWIN2::hideUi()
{
    const uint8_t commandLen = 8;
    // Выключаем отображение UI элемента
    uint8_t command[commandLen] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_spHexAddr);
    command[5] = lowByte(_spHexAddr);
    command[6] = 0xFF;
    command[7] = 0xFF;
    // Send data to uartTask
    sendUart(command, commandLen);
}

void DWIN2::blinkUI(bool state)
{
    uint8_t command[8] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    command[4] = highByte(_spHexAddr);
    command[5] = lowByte(_spHexAddr);
    if (state)
    {
        command[6] = highByte(_vpHexAddr);
        command[7] = lowByte(_vpHexAddr);
    }
    else
    {
        command[6] = 0xFF;
        command[7] = 0xFF;
    }
    if (xSemaphoreTake(_uartMutex, portMAX_DELAY) == pdTRUE)
    {
        while (_uart->available())
        {
            // clear display answers
            _uart->read();
        }
        _uart->write(command, 8);
        xSemaphoreGive(_uartMutex);
    }
}

void DWIN2::blinkTmr(void *arg)
{
    DWIN2 *dwp = static_cast<DWIN2*>(arg);
    static bool blinkFlag = false;
    if (_isBlink)
    {
        dwp->blinkUI(blinkFlag);
        // Invert flag
        blinkFlag = !blinkFlag;
    }
}

void DWIN2::uartTask(void *parameter)
{
    DWIN2* p_dwin = static_cast<DWIN2*>(parameter);
    if (p_dwin->_uartWriteSem == nullptr) 
    {
        Serial.printf("xSemaphoreCreateBinary ERR\n");
        return;
    }
    if (_uartMutex == nullptr) return;

    std::vector<uint8_t> cmd;
    std::vector<uint8_t> buf;
    buf.reserve(BUFSIZE);

    while (true) {
        if (xSemaphoreTake(p_dwin->_uartWriteSem, portMAX_DELAY) == pdTRUE)
        {
            if (p_dwin->_uartCmdBuffer.size() > BUFSIZE-1)
            {
                Serial.printf("_uartCmdBuffer Overhead!!!\n");
                p_dwin->_uartCmdBuffer.clear();
                continue;
            } 
            // Block access to the buffer when reading it
            if (xSemaphoreTake(_bufferMutex, portMAX_DELAY) == pdTRUE)
            {
                // Copy buffer from uart commands to local buffer
                buf = p_dwin->_uartCmdBuffer;
                // _uartCmdBuffer is no longer needed, reset to zero
                p_dwin->_uartCmdBuffer.clear();
                xSemaphoreGive(_bufferMutex);
            }

            // Buffer must be full
            if (buf.size() < 4) continue;

            // Split the buffer into separate commands
            for (int i = 0; i < buf.size(); i++)
            {
                // If we found header 0xA55A
                if (((buf[i] == 0x5A) && (buf[i+1] == 0xA5)) || (i == buf.size()-1))
                {
                    // Add the last byte
                    if (i == buf.size()-1) cmd.push_back(buf[i]);
                    // Skip the first two bytes
                    if (cmd.size() > 2)
                    {
                        // If the command header starts with 0xA55A, the command is assembled
                        if ((cmd[0] == 0x5A) && (cmd[1] == 0xA5))
                        {
                            // Block access to uart, send command via uart
                            if (xSemaphoreTake(_uartMutex, portMAX_DELAY) == pdTRUE)
                            {
                                for (int i = 0; i < cmd.size(); i++)
                                {
                                    _uart->write(cmd[i]);
                                }
                                xSemaphoreGive(_uartMutex);
                            }
                            // Wait for a response from the display
                            int indx = 0;
                            while (!_uart->available())
                            {
                                indx++;
                                vTaskDelay(pdMS_TO_TICKS(1));
                                // If no response is received, exit the loop
                                if (indx > 30) break;
                            }
                            // Clear the receive buffer
                            _uartRxBuf.clear();
                            // Block access to uart, read the response from the display
                            if (xSemaphoreTake(_uartMutex, portMAX_DELAY) == pdTRUE)
                            {
                                while (_uart->available())
                                {
                                    uint8_t d = static_cast<uint8_t>(_uart->read());
                                    _uartRxBuf.push_back(d);
                                }
                                xSemaphoreGive(_uartMutex);
                            }

                            // Give semaphore to read data from ui element
                            xSemaphoreGive(p_dwin->_uartUiReadSem);

                            // If echo mode is enabled
                            if (p_dwin->_echo)
                            {
                                String uartCmdStr = printHex(cmd, cmd.size());
                                String hexStr = printHex(_uartRxBuf, _uartRxBuf.size());
                                String idStr = (String)p_dwin->_id;
                                p_dwin->_dwinEcho = "ID" + idStr + " TX " + uartCmdStr + "\t RX " + hexStr;
                                // Send to callback
                                p_dwin->_handleEchoUart();
                            }
                            else
                            {
                                // Just clean the buffer
                                p_dwin->clearRxBuf();
                            }
                        }
                    }
                    // Command sent, clear cmd, start building a new command, if any
                    cmd.clear();
                }
                // Add a byte to the command
                cmd.push_back(buf[i]);
            }
            buf.clear();
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
}

String DWIN2::utf16_to_utf8(const uint16_t *utf16, size_t utf16_len)
{
    String utf8_str;
    for (size_t i = 0; i < utf16_len; ++i) {
        uint16_t unicode = utf16[i];
        if (unicode <= 0x7F) {
            utf8_str += (char)unicode;
        } else if (unicode <= 0x7FF) {
            utf8_str += (char)(0xC0 | (unicode >> 6));
            utf8_str += (char)(0x80 | (unicode & 0x3F));
        } else {
            utf8_str += (char)(0xE0 | (unicode >> 12));
            utf8_str += (char)(0x80 | ((unicode >> 6) & 0x3F));
            utf8_str += (char)(0x80 | (unicode & 0x3F));
        }
    }
    return utf8_str;
}

void DWIN2::setPage(const uint8_t &pageNum)
{
    const uint8_t commandLen = 10;
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 0x00, pageNum};
    // Send data to uartTask
    sendUart(command, commandLen);
}

uint8_t DWIN2::getPage()
{
    const uint8_t commandLen = 7;
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x14, 0x01};
    // Send data to uartTask
    sendUart(command, commandLen);
    while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
    std::vector<uint8_t> buf = _uartRxBuf;
    if (buf.size() >= 8) return buf.at(8);
    return 0;
}

void DWIN2::setBrightness(const uint8_t &brightness)
{
    uint8_t brtn = brightness;
    if (brtn > 127) brtn = 127;
    const uint8_t commandLen = 7;
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x82, brtn};
    // Send data to uartTask
    sendUart(command, commandLen);
}

uint8_t DWIN2::getBrightness()
{
    const uint8_t commandLen = 7;
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x31, 0x01};
    // Send data to uartTask
    sendUart(command, commandLen);
    while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
    std::vector<uint8_t> buf = _uartRxBuf;
    if (buf.size() >= 8) return buf.at(8);
    return 0;
}

void DWIN2::restartHMI()
{
    const uint8_t commandLen = 10;
    unsigned char command[commandLen] = {0x5A, 0xA5, 0x07, 0x82, 0x00, 0x04, 0x55, 0xAA, 0x5A, 0xA5};
    // Send data to uartTask
    sendUart(command, commandLen);
    delay(100);
}


uint8_t DWIN2::getVarIconIndex()
{
    if (_uitype != ICON) return 0;
    const uint8_t commandLen = 7;
    uint16_t num = 0;

    // Clear the receiving buffer of the display
    clearRxBuf();

    uint8_t command[commandLen] = {0x5A, 0xA5, 0x04, 0x83, 0x00, 0x00, 0x00};

    command[4] = highByte(_vpHexAddr);
    command[5] = lowByte(_vpHexAddr);
    command[6] = 0x01;

    // Send data to uartTask
    sendUart(command, commandLen);
    while (xSemaphoreTake(_uartUiReadSem, pdMS_TO_TICKS(100)) == pdTRUE){};
    std::vector<uint8_t> buffer = _uartRxBuf; // Копируем полученные данные в буфер
    if ((buffer[3] == 0x83) && (buffer[4] == highByte(_vpHexAddr)) && (buffer[5] == lowByte(_vpHexAddr)))
    {
        num = static_cast<uint16_t>(buffer[7] << 8) | static_cast<uint16_t>(buffer[8]);
    }
    return num;
}