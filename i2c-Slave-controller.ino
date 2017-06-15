#include <Ultrasonic.h>   // Ультразвуковые датчики HC-SR04
#include <Wire.h>         // I2C
#include <max6675.h>      // Термопара

/* Указание портов шины SPI для MAX6675 */
#define thermoDO 10
#define thermoCS 11
#define thermoCLK 12

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

/* Используем для питания max6675 пины Arduino */
#define vccPin 3
#define gndPin 2

/* Временно запишем значения термисторов при разных температурах

  864 - 85 град.
  617 - 40 град.
  368 - 11 град.

*/

/* Переменные для уровня воды в баке с горячей водой в бане (используем A0 аналоговый вход) */
#define analog0Pin 0
int analog0      = 0;

/* Переменные для терморезистора в парилке (используем A1 аналоговый вход) */
#define analog1Pin 1
int termo_sensor1          = 0;      // Переменная для чтения текущих показаний датчика прозрачности

/* Переменные для датчика прозрачности в накопительных емкостях (используем A2 аналоговый вход) */
#define analog2Pin 2
int analog2         = 0;      // Переменная для чтения текущих показаний датчика прозрачности
int analog2_temp    = 0;      // Временная переменная для усреднения показаний датчика
int analog2_percent = 0;      // Процент прозрачности

/* Переменные для терморезистора горячей воды в парилке (используем A3 аналоговый вход) */
#define analog3Pin 3
int termo_sensor2       = 0;      // Переменная для чтения текущих показаний датчика прозрачности

/* Переменные для терморезистора в парилке (используем A1 аналоговый вход) */
#define analog6Pin 6
int termo_sensor3          = 0;      // Переменная для чтения текущих показаний датчика прозрачности

const int averageFactor = 5;      // коэффициент сглаживания показаний (0 = не сглаживать) - чем выше, тем больше "инерционность" - это видно на индикаторах

// Инициализация 1-го ультразвукового датчика (Trig, Echo)
Ultrasonic ultrasonic1(8, 9, 8700);

// Инициализация 2-го ультразвукового датчика (Trig, Echo)
Ultrasonic ultrasonic2(6, 7, 8700);

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change :
const long interval = 3000;           // interval at which to blink (milliseconds)

/* Массив int для хранения результатов опроса датчиков для последующей передачи в Master-контроллер через i2c
  По порядку элементов:
  0 - расстояние в см. на ultrasonic1
  1 - расстояние в см. на ultrasonic2
  2 - уровень воды для бака с горячей водой
  3 - температура термопары (целая часть)
  4 - температура термопары (дробная часть)
  5 - процент прозрачности воды в накопительной емкости
  6 - температура воздуха в парилке
  7 - температура воды в баке
  8 - температура улицы
  9 - температура в доме
*/
uint8_t SlaveResult[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* Переменная с типом float для получения температуры и переменные для целой и дробной частей температуры с MAX6675 */
float max6675temp = 0;        // Переменная для чтения текущий показаний термопары

/* Две переменных для разделения float на целую и дробную части, т.к. массив, передаваемый на мастер не понимает float */
int dataA         = 0;        // Переменная для хранения целой части от значения float-переменной
int dataB         = 0;        // Переменная для хранения дробной части от значения float-переменной

int HotWaterTankLevel       = 0;          // Уровень воды в % (долях) для бака с горячей водой

void setup() {

  Serial.begin(9600);

  /* Выход на светодиод наличия связи с Master (LINK) */
  pinMode(LED_BUILTIN, OUTPUT);

  /* Инициализация MAX6675 - термопара */
  pinMode(vccPin, OUTPUT); digitalWrite(vccPin, HIGH);
  pinMode(gndPin, OUTPUT); digitalWrite(gndPin, LOW);
  /* Подождем стабилизации чипа MAX6675 */
  Serial.println("MAX6675 test...");
  delay(1000);
  Serial.println("MAX6675 ready.");

  /* Инициализация Slave-контроллера на 8 адресе i2c */
  Wire.begin(8);                    // join i2c bus with address #8
  Wire.onRequest(requestEvent);     // register event
  Serial.println("i2c Slave started on 8 addr.");

  /* Initialize DS3231 */
  //Serial.println("Initialize DS3231");
  //clock.begin();
}

void loop() {

  // Проверка уровней воды в емкостях ультразвуковыми датчиками
  MainTankLevelCheck();
  StreetTankLevelCheck();

  // Проверка уровня воды в емкости с горячей водой (4 положения уровня)
  HotWaterTankLevelCheck();

  // Проверка прозрачности воды в накопительных емкостях
  WaterQuality_StreetTank();

  // Проверка температуры воздуха в парилке
  SaunaAirTermperature();

  // Проверка температуры воды в парилке
  SaunaWaterTermperature();

  // Проверка температуры с термопары на MAX6675 (температура в баке с горячей водой)
  SaunaStonesTempCheck();

  // Проверка температуры воздуха
  StreetTemperature();

  // Link OFF
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
}

// Функция зарегистрированная как событие в секции setup() wire.onRequest
// отправка в i2c массива с результатами замеров датчиков
void requestEvent()
{
  Wire.write(SlaveResult, 10);        // ответ на запрос от Master-контроллера
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
}

// Получение уровня домашней емкости в см.
void MainTankLevelCheck()
{
  SlaveResult[0] = ultrasonic1.Ranging(CM);
  //Serial.println(ultrasonic1.Ranging(CM));       // debug msg.
}

// Получение уровня уличной емкости в см.
void StreetTankLevelCheck()
{
  SlaveResult[1] = ultrasonic2.Ranging(CM);
  //  Serial.println("US2:");
  //  Serial.println(ultrasonic2.Ranging(CM));        // debug msg.

}

// Получение уровня бака с горячей водой в %
// 4 фиксированных положения "пусто = 0", "минимум = 1", "середина = 2", "максимум = 3"
void HotWaterTankLevelCheck()
{
  analog0 = analogRead(analog0Pin);
  if (analog0 < 300)
  {
    HotWaterTankLevel = 3;  //пусто
  }

  if ((analog0 > 300) && (analog0 < 600))
  {
    HotWaterTankLevel = 2;  //минимум
  }

  if ((analog0 > 600) && (analog0 < 1000))
  {
    HotWaterTankLevel = 1;  //середина
  }

  if (analog0 > 1000)
  {
    HotWaterTankLevel = 0;  //максимум
  }

  SlaveResult[2] = HotWaterTankLevel;         // Пишем в 3-й элемент массива значение уровня емкости бака с горячей водой
}

/* Чтение показаний термопары */
void SaunaStonesTempCheck()
{
  int air_stone_temp_diff = 0;

  /* Получаем значение с типом float и разбираем его на целую и дробную части для передачи Master-контроллеру через i2c */
  max6675temp = thermocouple.readCelsius();
  delay(250);                                 // Задерка, как выяснилось, обязательна, иначе не обновляется значение переменной (работает от 150 но для большей уверенности поднял до 250)
  //Serial.println(max6675temp);

  // Компенсируем показания термопары через температуру воздуха в парилке
  air_stone_temp_diff = max6675temp - termo_sensor1;

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

  SlaveResult[3] = dataA;             // Верхний байт значения температуры
  SlaveResult[4] = (dataA >> 8);      // Нижний байт значения температуры

  /* Отладочные сообщения */
  //  int exper = (SlaveResult[4] << 8)|SlaveResult[3];
  //  Serial.println(exper);
  //  Serial.println(dataA);
}

/* Читаем показания датчика прозрачности в накопительных емкостях
  датчик работает по принципу - чем выше прозрачность, тем большее выходное напряжение на входе (от 0V до 5V) */
void WaterQuality_StreetTank()
{
  //int oldsensorValue = analog2_temp;

  analog2 = analogRead(analog2Pin);
  //  Serial.print("RAW: ");
  //  Serial.println(analog2);

  //analog2_temp = (oldsensorValue * (averageFactor - 1) + analog2) / averageFactor;
  analog2_percent = map(analog2, 0, 1023, 0, 100);// + (averageFactor - 1);

  //  Serial.print("average: ");
  //  Serial.println(analog2_temp);
  //  Serial.print("%: ");
  //  Serial.println(analog2_percent);

  SlaveResult[5] = analog2_percent;   // Прозрачность воды в накопительных емкостях пишем в 6-й элемент массива
}

void SaunaAirTermperature()
{
  termo_sensor1 = analogRead(analog1Pin);
  //Serial.println(termo_sensor1);
  termo_sensor1 = map(termo_sensor1, 368, 864, 11, 85);
  //Serial.println(termo_sensor1);

  SlaveResult[6] = termo_sensor1;
}

void SaunaWaterTermperature()
{
  termo_sensor2 = analogRead(analog3Pin);
  //Serial.println(termo_sensor2);
  termo_sensor2 = map(termo_sensor2, 368, 864, 11, 85);
  //Serial.println(termo_sensor2);

  SlaveResult[7] = termo_sensor2;
}

void StreetTemperature()
{
  termo_sensor3 = analogRead(analog6Pin);
  //Serial.println(termo_sensor3);
  termo_sensor3 = map(termo_sensor3, 368, 864, 11, 85);
  //Serial.println(termo_sensor3);

  SlaveResult[8] = termo_sensor3;
}
