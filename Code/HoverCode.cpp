#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Adafruit_NeoPixel.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// Пины для подключения
#define SERVO_PIN 21
#define SINGLE_FAN_PIN 22
#define DUAL_FAN1_PIN 19
#define DUAL_FAN2_PIN 2
#define LED_STRIP_PIN 16 // Пин для светодиодной ленты на GPIO2
#define NUM_LEDS 3       // Количество светодиодов

// Диапазоны импульсов
#define MIN_PULSE 1000
#define MAX_PULSE 2000

// Объекты для управления
Servo servo;
Servo singleFan;
Servo dualFan1;
Servo dualFan2;
Adafruit_NeoPixel ledStrip = Adafruit_NeoPixel(NUM_LEDS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

// Переменные состояния
int currentServoAngle = 90;
int currentSingleFanSpeed = 0;
int currentDualFanSpeed = 0;
bool ledStripOn = false; // Включена ли светодиодная лента

void onConnectedController(ControllerPtr ctl)
{
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (myControllers[i] == nullptr)
        {
            Serial.printf("Controller connected, index=%d\n", i);
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot)
    {
        Serial.println("Controller connected, but no empty slot");
    }
}

void onDisconnectedController(ControllerPtr ctl)
{
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (myControllers[i] == ctl)
        {
            Serial.printf("Controller disconnected from index=%d\n", i);
            myControllers[i] = nullptr;
            break;
        }
    }
}

// Функции управления устройствами
void setServoAngle(int angle)
{
    angle = constrain(angle, 0, 180);
    if (angle != currentServoAngle)
    {
        currentServoAngle = angle;
        servo.write(angle);
    }
}

void setSingleFanSpeed(int speed)
{
    speed = constrain(speed, 0, 100);
    if (speed != currentSingleFanSpeed)
    {
        currentSingleFanSpeed = speed;
        int pulse = map(speed, 0, 100, MIN_PULSE, MAX_PULSE);
        singleFan.writeMicroseconds(pulse);
    }
}

void setDualFanSpeed(int speed)
{
    speed = constrain(speed, 0, 100);
    if (speed != currentDualFanSpeed)
    {
        currentDualFanSpeed = speed;
        int pulse = map(speed, 0, 100, MIN_PULSE, MAX_PULSE);
        dualFan1.writeMicroseconds(pulse);
        dualFan2.writeMicroseconds(pulse);
    }
}

// Простое включение/выключение светодиодной ленты
void toggleLedStrip()
{
    ledStripOn = !ledStripOn;

    if (ledStripOn)
    {
        // Включаем все светодиоды белым цветом
        for (int i = 0; i < NUM_LEDS; i++)
        {
            ledStrip.setPixelColor(i, ledStrip.Color(255, 255, 255));
        }
        ledStrip.show();
        Serial.println("LED Strip: ON (White)");
    }
    else
    {
        // Выключаем все светодиоды
        ledStrip.clear();
        ledStrip.show();
        Serial.println("LED Strip: OFF");
    }
}

void processGamepad(ControllerPtr ctl)
{
    static bool lastTriangleState = false;

    // 1. Левый стик - управление сервоприводом
    int leftStickX = ctl->axisX();
    if (leftStickX <= -25)
    {
        int angle = map(leftStickX, -511, -25, 0, 90);
        setServoAngle(angle);
    }
    else if (leftStickX >= 25)
    {
        int angle = map(leftStickX, 25, 512, 90, 180);
        setServoAngle(angle);
    }
    else
    {
        setServoAngle(90);
    }

    // 2. R2 - скорость одиночного вентилятора
    int r2Value = ctl->brake();
    if (r2Value > 10)
    {
        int speed = map(r2Value, 0, 1023, 0, 100);
        setSingleFanSpeed(speed);
    }
    else
    {
        setSingleFanSpeed(0);
    }

    // 3. Кнопки для парных вентилей
    if (ctl->buttons() & 0x0001)
    { // Крестик - 20%
        setDualFanSpeed(20);
    }
    if (ctl->buttons() & 0x0004)
    { // Квадратик - 30%
        setDualFanSpeed(50);
    }
    if (ctl->buttons() & 0x0002)
    { // Кружок - 50%
        setDualFanSpeed(80);
    }

    // 4. Треугольник - вкл/выкл светодиодную ленту
    bool currentTriangleState = (ctl->buttons() & 0x0008) != 0;
    if (currentTriangleState && !lastTriangleState)
    {
        toggleLedStrip();
    }
    lastTriangleState = currentTriangleState;

    // 5. L1 - остановка всех вентиляторов
    if (ctl->buttons() & 0x0010)
    {
        setSingleFanSpeed(0);
        setDualFanSpeed(0);
    }
}
void processControllers()
{
    for (auto myController : myControllers)
    {
        if (myController && myController->isConnected() &&
            myController->hasData())
        {
            if (myController->isGamepad())
            {
                processGamepad(myController);
            }
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting...");

    // Инициализация светодиодной ленты
    ledStrip.begin();
    ledStrip.setBrightness(150); // Средняя яркость
    ledStrip.clear();
    ledStrip.show();

    // Инициализация сервопривода и моторов
    servo.attach(SERVO_PIN);
    singleFan.attach(SINGLE_FAN_PIN);
    dualFan1.attach(DUAL_FAN1_PIN);
    dualFan2.attach(DUAL_FAN2_PIN);

    // Калибровка ESC
    delay(1000);
    singleFan.writeMicroseconds(MAX_PULSE);
    dualFan1.writeMicroseconds(MAX_PULSE);
    dualFan2.writeMicroseconds(MAX_PULSE);
    delay(1000);
    singleFan.writeMicroseconds(MIN_PULSE);
    dualFan1.writeMicroseconds(MIN_PULSE);
    dualFan2.writeMicroseconds(MIN_PULSE);
    delay(2000);

    // Начальные состояния
    setServoAngle(90);

    // Настройка Bluepad32
    BP32.setup(&onConnectedController, &onDisconnectedController);
    BP32.forgetBluetoothKeys();
    BP32.enableVirtualDevice(false);

    Serial.println("=== READY ===");
    Serial.println("Triangle: LED Strip ON/OFF");
}

void loop()
{
    BP32.update();
    processControllers();
    delay(50);

}
