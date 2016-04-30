#include <Ultrasonic.h>   // Ультразвуковые датчики HC-SR04
#include <Wire.h>         // I2C
#include <max6675.h>      // Термопара

/* Указание портов шины SPI для MAX6675 */
#define thermoDO 10
#define thermoCS 11
#define thermoCLK 12

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

/* Используем для питания пины Arduino */
int vccPin = 3;
int gndPin = 2;

// Инициализация 1-го ультразвукового датчика (Trig, Echo)
Ultrasonic ultrasonic1(8, 9, 8700);

// Инициализация 2-го ультразвукового датчика (Trig, Echo)
Ultrasonic ultrasonic2(6, 7, 8700);

/* Массив int для хранения результатов опроса датчиков для последующей передачи в Master-контроллер через i2c
  По порядку элементов:
  0 - расстояние в см. на ultrasonic1
  1 - расстояние в см. на ultrasonic2
  2 - уровень воды для бака с горячей водой
  3 - температура термопары (целая часть)
  4 - температура термопары (дробная часть)
*/
uint8_t SlaveResult[5] = {0, 0, 0, 0, 0};

//int ultrasonic1_cm          = 0;          // Расстояние от ультразвукового датчика до поверхности воды в домашней емкости (в см.)
//int ultrasonic2_cm          = 0;          // Расстояние от ультразвукового датчика до поверхности воды в уличной емкости (в см.)
//int HotWater_Temp           = 0;          // Температура горячей воды (показания термопары)

/* Переменная с типом float для получения температуры и переменные для целой и дробной частей температуры с MAX6675 */
float max6675temp = 0;
int dataA         = 0;
int dataB         = 0;

int HotWaterTankLevel       = 0;          // Уровень воды в % (долях) для бака с горячей водой

void setup() {

  Serial.begin(9600);

  /* Инициализация MAX6675 - термопара */
  pinMode(vccPin, OUTPUT); digitalWrite(vccPin, HIGH);
  pinMode(gndPin, OUTPUT); digitalWrite(gndPin, LOW);
  /* Подождем стабилизации чипа MAX6675 */
  Serial.println("MAX6675 test...");
  delay(500);
  Serial.println("MAX6675 ready.");

  /* Инициализация Slave-контроллера на 8 адресе i2c */
  Wire.begin(8);                    // join i2c bus with address #8
  Wire.onRequest(requestEvent);     // register event
  Serial.println("i2c Slave started on 8 addr.");

}

void loop() {

  // Проверка уровней воды в емкостях ультразвуковыми датчиками
  MainTankLevelCheck();
  StreetTankLevelCheck();

  // Проверка уровня воды в емкости с горячей водой (4 положения уровня)
  HotWaterTankLevelCheck();

  // Проверка температуры с термопары на MAX6675 (температура в баке с горячей водой)
  HotWaterTempCheck();

}

// Функция зарегистрированная как событие в секции setup() wire.onRequest
// отправка в i2c массива с результатами замеров датчиков
void requestEvent()
{
  Wire.write(SlaveResult, 5);        // ответ на запрос от Master-контроллера
}

// Получение уровня домашней емкости в см.
void MainTankLevelCheck()
{
  SlaveResult[0] = ultrasonic1.Ranging(CM);
}

// Получение уровня уличной емкости в см.
void StreetTankLevelCheck()
{
  SlaveResult[1] = ultrasonic2.Ranging(CM);
}

// Получение уровня бака с горячей водой в %
// 4 фиксированных положения "пусто = 0", "минимум = 1", "середина = 2", "максимум = 3"
void HotWaterTankLevelCheck()
{
  HotWaterTankLevel = 2;  // пока нет готового датчика временно передаем значение 2
  SlaveResult[2] = HotWaterTankLevel;
  //HotWaterTankLevel = 0;  //пусто
  //HotWaterTankLevel = 1;  //минимум
  //HotWaterTankLevel = 2;  //середина
  //HotWaterTankLevel = 3;  //максимум
}

/* Чтение показаний термопары */
void HotWaterTempCheck()
{
  /* Получаем значение с типом float и разбираем его на целую и дробную части для передачи Master-контроллеру через i2c */
  max6675temp = thermocouple.readCelsius();
  delay(250);
  //Serial.println(max6675temp);

  /* Целая часть значения температуры */
  dataA = int(max6675temp);

  /* Дробная часть значения температуры (до десятых долей) - условия в зависимости от знака */
  if (max6675temp <= 0)
  {
    dataB = (int(max6675temp) - max6675temp) * 10;
  }
  else
  {
    dataB = (max6675temp - int(max6675temp)) * 10;
  }

  SlaveResult[3] = dataA;     // Целая часть значения температуры
  SlaveResult[4] = dataB;     // Дробная часть значения температуры
}
