#include <Ultrasonic.h>   // Ультразвуковые датчики HC-SR04
#include <Wire.h>         // I2C
#include <max6675.h>      // Термопара

/* Указание портов шины SPI для MAX6675 */
#define thermoDO 10
#define thermoCS 11
#define thermoCLK 12

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

/* Используем для питания пины Arduino */
#define vccPin 3
#define gndPin 2

/* Переменные для уровня воды в баке с горячей водой в бане */
#define analog0Pin 0
int analog0      = 0;

/* Переменные для датчика прозрачности в накопительных емкостях */
#define analog1Pin 2
int analog1         = 0;
int analog1_temp    = 0;
int analog1_percent = 0;

const int averageFactor = 5;               // коэффициент сглаживания показаний (0 = не сглаживать) - чем выше, тем больше "инерционность" - это видно на индикаторах

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
uint8_t SlaveResult[6] = {0, 0, 0, 0, 0, 0};

// 0 - Расстояние от ультразвукового датчика до поверхности воды в домашней емкости (в см.)
// 1 - Расстояние от ультразвукового датчика до поверхности воды в уличной емкости (в см.)
// 2 - Уровень в емкости с горячей водой
// 3 - Температура горячей воды целые (показания термопары)
// 4 - Температура горячей воды десятые (показания термопары)
// 5 - Прозрачность воды в накопительной емкости

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

  // Проверка прозрачности воды в накопительных емкостях
  WaterQuality_StreetTank();

}

// Функция зарегистрированная как событие в секции setup() wire.onRequest
// отправка в i2c массива с результатами замеров датчиков
void requestEvent()
{
  Wire.write(SlaveResult, 6);        // ответ на запрос от Master-контроллера
}

// Получение уровня домашней емкости в см.
void MainTankLevelCheck()
{
  SlaveResult[0] = ultrasonic1.Ranging(CM);
  //Serial.println(SlaveResult[0]);
}

// Получение уровня уличной емкости в см.
void StreetTankLevelCheck()
{
  SlaveResult[1] = ultrasonic2.Ranging(CM);
  //Serial.println(ultrasonic2.Ranging(CM));
}

// Получение уровня бака с горячей водой в %
// 4 фиксированных положения "пусто = 0", "минимум = 1", "середина = 2", "максимум = 3"
void HotWaterTankLevelCheck()
{
  analog0 = analogRead(analog0Pin);
  //Serial.println(analog0);
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

  SlaveResult[2] = HotWaterTankLevel;
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

void WaterQuality_StreetTank()
{
  int oldsensorValue = analog1_temp;

  analog1 = analogRead(analog1Pin);
  //  Serial.print("RAW: ");
  //  Serial.println(analog1);

  analog1_temp = (oldsensorValue * (averageFactor - 1) + analog1) / averageFactor;
  analog1_percent = map(analog1_temp, 0, 1023, 0, 100) + (averageFactor - 1);

  //  Serial.print("average: ");
  //  Serial.println(analog1_temp);
  //  Serial.print("%: ");
  //  Serial.println(analog1_percent);

  SlaveResult[5] = analog1_percent;   // Прозрачность воды в накопительных емкостях
}
