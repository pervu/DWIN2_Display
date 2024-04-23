#include <Arduino.h>
#include <Dwin2.h>

//*****************************************************************//
// You can use Class like One object - one DWIN display Element  **//
// Use methods like in the QuickStart example                    **//
//*****************************************************************//

// Rx Tx ESP gpio connected to DWin Display
#define RX_PIN 16
#define TX_PIN 17
#define UIELEM_QTY 4

// Pointer dwin array of controlling UI elements of the display
DWIN2 *dwc[UIELEM_QTY];

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

    // VP SP address arrays
    const uint16_t vpArr[UIELEM_QTY] = {0x1000, 0x1010, 0x1020, 0x1030};
    const uint16_t spArr[UIELEM_QTY] = {0x9000, 0x9010, 0x9020, 0x9030};
    // Инициализируем общение с дисплеем 
    for (int i = 0; i < UIELEM_QTY; i++)
    {
        // Создание объекта для работы с дисплеем
        dwc[i] = new DWIN2;
        // Init timers, tasks, serial communication and other
        dwc[i]->begin(spArr[i], vpArr[i], RX_PIN, TX_PIN);
        // Set callback for answer
        dwc[i]->setUartCbHandler(dwinEchoCallback);
        // Show commands and answers
        dwc[i]->setEcho(true);
    }


//----------------------------------------------------------------------------------------  

    // General commands that refer to the display and not its UI elements
    // You can use any object
    // Set display pages
    dwc[0]->setPage(2);
    delay(d);
    dwc[0]->setPage(0);
    delay(d);

    // Send raw command
    const uint8_t rawCmd1[] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x82, 0x00};       // Set display brightness to 0%
    const uint8_t rawCmd2[] = {0x5A, 0xA5, 0x04, 0x82, 0x00, 0x82, 0x7F};       // Set display brightness to 100%
    dwc[0]->sendRawCommand(rawCmd1, sizeof(rawCmd1));
    delay(d);
    dwc[0]->sendRawCommand(rawCmd2, sizeof(rawCmd2));
    delay(d);

//----------------------------------------------------------------------------------------

    // Restart HMI
    //dwc.restartHMI();
    delay(1000);
    Serial.printf("----- Dwin Display common commands end -----\n");

//----------------------------------------------------------------------------------------

    Serial.printf("\n----- Dwin UI commands start -----\n");
    // Set ui type of the display elemet communicating with
    dwc[0]->setUiType(INT);
    // Set text color
    dwc[0]->setColor(SKY_BLUE);
    delay(d);
    // Send data
    dwc[0]->sendData(55);

    // Set utf UI type
    dwc[3]->setUiType(UTF);      // Set type of the UI element of the dwin display
    // Send UTF text to display
    dwc[3]->sendData("UTF Текст");

//----------------------------------------------------------------------------------------

    // Set new UI type
    dwc[1]->setUiType(DOUBLE);      // Set type of the UI element of the dwin display
    // Set blink period
    Serial.printf("Blink 200\n");
    dwc[1]->setBlinkPeriod(200);
    // Start blink
    dwc[1]->blink(true);
    delay(2000);
    // Stop blinking
    dwc[1]->blink(false);
    delay(d);

    Serial.printf("\n--------------------------------------------------\n");
    Serial.printf("-------- DWIN communication demo finished --------\n");
}


void loop() {
    delay(portMAX_DELAY);
}

void dwinEchoCallback(DWIN2 &d)
{
    Serial.print("ID# ");
    Serial.print(d.getId());
    Serial.print(" echo: ");
    Serial.println(d.getDwinEcho());
}
