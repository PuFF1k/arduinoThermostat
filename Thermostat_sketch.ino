#include <LiquidCrystal_I2C.h>
//#include <DS1302.h>
#include <DS3231.h>
#include <math.h>
#include <AH_24Cxx.h>
#include <Wire.h> 
#include "DHT.h"
#define DHTPIN 12
#define DHTTYPE DHT11
#define AT24C08  3
//DS1302 rtc(8, 7, 6);
DS3231  rtc(SDA, SCL);
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address
#define BUSADDRESS  0x00
#define EEPROMSIZE  1024     //AT24C16 2048byte

//ic_eeprom.write_byte(1023,111);
//ic_eeprom.read_byte(1023);

AH_24Cxx ic_eeprom = AH_24Cxx(AT24C08, BUSADDRESS);
char clear[] = "                    ";
DHT dht(DHTPIN, DHTTYPE);
int pinA = 2; // Пины прерываний
int pinB = 3; // Пины прерываний

Time t;
int secs;
bool PochatkovyEkran;
bool kotelHisteresis = false;
bool rozetkaHisteresis = false;
int knopkaenkoder;
int knopkanazad;
int nomer_ryadka = 0;
int nomer_ryadka_zminy_parametra = 0;
int rezym;

volatile long pause = 50;  // Пауза для борьбы с дребезгом
volatile long lastTurn = 0;   // Переменная для хранения времени последнего изменения

volatile int count = 0;       // Счетчик оборотов
int actualcount = 0;       // Временная переменная определяющая изменение основного счетчика

volatile int state = 0;       // Статус одного шага - от 0 до 4 в одну сторону, от 0 до -4 - в другую

volatile int pinAValue = 0;   // Переменные хранящие состояние пина, для экономии времени
volatile int pinBValue = 0;   // Переменные хранящие состояние пина, для экономии времени

                // Переменные хранящие состояние действия до его выполнения
volatile bool flagNext = false;     // Было ли вращение по часовой стрелке
volatile bool flagPrev = false;     // Было ли вращение против часовой стрелки
volatile bool flagButton = false;     // Было ли нажатие кнопки
volatile bool flagButtonBack = false;     // Было ли долгое удержание кнопки

/*int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31}

void calibrateCalendar()
{
  int nowYear = t.year;
  bool leapYear = false;
  if (nowYear % 4 != 0){}
  else if (nowYear % 100 != 0) {leapYear = true;}
  else if (nowYear % 400 != 0) {}
  else {leapYear = true;}

  if()
  {
  }
}*/

String rezymy[] =
{
  "po temperaturi",
  "po godynam"
};

//в пзп буде зберігатися так : перші 3 байта(перша строка) : 50,51,52 і наступні 3 байта для наступної строки
bool robochiGodyny[2][24] =
{
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

struct menu                             // Структура описывающая меню
{
  int id;                               // Идентификационный уникальный индекс ID
  int parentid;                         // ID родителя
  int Param;                         // Является ли пункт изменяемым параметром
  char name[21];                         // Название
  int value;                            // Актуальное значение
  int _min;                             // Минимально возможное значение параметра
  int _max;                             // Максимально возможное значение параметра
};




/*крч Парам це:
-1 - просто відображає початковий екран
0 - елемент меню який містить інші елементи
1 - елемент меню що містить 1 змінну з можливістю її модифікації
2 - елемент меню що містить три змінних типу час і дата з можливістю їх модифікації
3 - елемент меню що містить змінну днів тижня
5 - елемент для вибору певних годин з діапазону 24 годин мусе бути можливість модифікувати аррей булів
щось типу такого на екран виводити
0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 23
+ - + - - - - + + + +  +  +  +  -  -  -  -  -  -  -  +  + 
*/
int menuArraySize = 20;                 // Задаем размер массива
menu menus[] = {                        // Задаем пункты меню
  { 0, -1, -1,"Main                ", 0, 0, 0 },
  { 1, 0, 0,"Menu Podiy          ", 0, 0, 0 },
  { 2, 1, 0,"Nichniy rezym       ", 0, 0, 23 },
  { 3, 2, 1,"Chas do vkl         ", 0, 0, 23 },
  { 4, 1, 0,"Rezym vidsutnosti   ", 0, 0, 0 },
  { 5, 4, 1,        "Chas do vkl         ", 0, 0, 23},
  { 6, 0, 0,    "Menu nalashtuvan    ", 0, 0, 0 },
  { 7, 6, 0,      "Data i chas         ", 0, 0, 0 },
  { 8, 7, 2,        "Chas                ", 0, 0, 60 },
  { 9, 7, 2,        "Data                ", 0, 0, 2200 },
  { 10, 6, 0,     "Nalashtuvania kotla ", 0, 0, 0 },
  { 11, 10, 3,        "Sposib vkl          ", 0, 0, 1 },
  { 12, 10, 1,        "Sposib po temp      ", 40, -20, 50 },
  { 13, 10, 4,        "Robochi hodyny      ", 0, 0, 23 },
  { 14, 10, 1,        "Histeresis          ", 0, 0, 24 },
  { 15, 6, 0,     "Nalashtuvania elekt ", 0, 0, 0 },
  { 16, 15, 3,        "Sposib vkl          ", 0, 0, 1 },
  { 17, 15, 1,        "Sposib po temp      ", 22, -20, 50 },
  { 18, 15, 4,        "Robochi hodyny      ", 0, 0, 23 },
  { 19, 15, 1,        "Histeresis          ",0, 0, 24 },
};                     
int actualIndex = 0;


String getDayofweek(unsigned int y, unsigned int m, unsigned int d)  /* 1 <= m <= 12,  y > 1752 (in the U.K.) */
{
  String day[] = 
  {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
  };
  static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  return day[(y + y/4 - y/100 + y/400 + t[m-1] + d) % 7];
}


int lcdwrite(float temp) 
{
  if (t.sec != secs) 
  {
    lcd.setCursor(6, 0);
    lcd.print(rtc.getTimeStr());
    lcd.setCursor(5, 1);
    lcd.print(rtc.getDateStr());
    lcd.setCursor(0, 2);
    lcd.print(getDayofweek(t.year, t.mon, t.date));
    lcd.print(", ");
    lcd.print(rtc.getMonthStr());
    lcd.print("    ");
    lcd.setCursor(1, 3);
    lcd.print("tmp ");
    lcd.print(temp);
    lcd.print("hmd ");
    lcd.print(dht.readHumidity());
  }
}


void saveBitToMemory(bool bit, bool kotelChyRozetka, int bitNumber)
{
  int memmoryPlace =0;
  switch(bitNumber) 
  {
    case 0 ... 7:
    memmoryPlace =0;
    break;
    case 8 ... 15:
    memmoryPlace =1;
    break;
    case 16 ... 24:
    memmoryPlace =2;
    break;
  }
  byte oldByte = ic_eeprom.read_byte(50 + memmoryPlace + (3*kotelChyRozetka));
  oldByte^= (-bit ^ oldByte) & (1UL << bitNumber-(8*memmoryPlace));
  ic_eeprom.write_char(50 + memmoryPlace + (3*kotelChyRozetka),oldByte);
}



void setup()
{
  lcd.begin(20, 4);
  Wire.begin();
  dht.begin();
  rtc.begin();
  lcdwrite(dht.readTemperature());
  actualIndex = getMenuIndexByID(0);
  pinMode(pinA, INPUT);           // Пины в режим приема INPUT
  pinMode(pinB, INPUT);           // Пины в режим приема INPUT

                  //кнопки
  pinMode(9, INPUT);
  pinMode(10, INPUT);

  //реле
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);

  attachInterrupt(0, A, CHANGE);  // Настраиваем обработчик прерываний по изменению сигнала
  attachInterrupt(1, B, CHANGE);  // Настраиваем обработчик прерываний по изменению сигнала
  
  //rtc.halt(false);
  //rtc.writeProtect(false);
  Serial.begin(9600);             // Включаем Serial
  setActualMenu(0, 0);
  
  for(int i=0;i<20;i++)
  {
    if(menus[i].Param == 1 || menus[i].Param == 3)
    {
      if(menus[i].id != 3)
      {
        menus[i].value = ic_eeprom.read_char(i);
      }
    }
  }
  
  for(int i = 0;i<6;i++)
  {
    byte byteFromMemory = ic_eeprom.read_byte(i+50);
    byte bitCounter =0;
    for(int j = i*8;j<8*(i+1);j++)
    {
      if(i<3)
      {
        robochiGodyny[0][j] = ((byteFromMemory >> bitCounter)  & 0x01);
      }
      else
      {
        robochiGodyny[1][j-24] = ((byteFromMemory >> bitCounter)  & 0x01);
      }
      bitCounter ++;
    }
  }
}

void loop()
{
  int vmenu = 0;                        // Переменная хранящая действие по вертикали 1 - вход в меню, -1 - выход из меню
  int hmenu = 0;                        // Переменная хранящая действие по горизонтали 1 - вправо, -1 - влево

  //double oldTemp = Thermister(analogRead(0));

  PochatkovyEkran == true;

  
  //Serial.println(ic_eeprom.read_byte(1023));
  
  //кнопки назад і вперед по меню(читаємо з усуненням дребезгу)
  if (knopkaenkoder == 0 && digitalRead(9) == HIGH) 
  { knopkaenkoder = 1; }

  if (knopkanazad == 0 && digitalRead(10) == HIGH) 
  { knopkanazad = 1; }

  if (knopkaenkoder == 1) 
  { Serial.println("KNOPKA ENCODERA");  flagButton = true; }

  if (knopkanazad == 1) 
  { Serial.println("KNOPKA NAZAD");  flagButtonBack = true;}



  if (flagNext) {               // Шаг вращения по часовой стрелке
    hmenu = 1;
    //Serial.println("right");
    flagNext = false;           // Действие обработано - сбрасываем флаг
  }
  if (flagPrev) {              // Шаг вращения против часовой стрелки
    hmenu = -1;
    //Serial.println("left");
    flagPrev = false;          // Действие обработано - сбрасываем флаг
  }
  if (flagButton) {           // Кнопка нажата
    vmenu = 1;                // По нажатию кнопки - переходим на уровень вниз
                  //Serial.println("button");
    flagButton = false;       // Действие обработано - сбрасываем флаг
  }

  if (flagButtonBack) {           // Кнопка нажата
    vmenu = -1;                // По нажатию кнопки - переходим на уровень вниз
                   //Serial.println("button");
    flagButtonBack = false;       // Действие обработано - сбрасываем флаг
  }
  if (vmenu != 0 || hmenu != 0) setActualMenu(vmenu, hmenu);

  //меню
  if (t.hour != rtc.getTime().hour && menus[3].value > 0) { menus[3].value--; }

  t = rtc.getTime();

  //Serial.println(PochatkovyEkran);

  float newTemp = dht.readTemperature();
  //Serial.println(dht.readTemperature());

  
  if (PochatkovyEkran == true)
  {
    lcdwrite(newTemp);
    secs = t.sec;
  }

  if (digitalRead(9) == HIGH&&knopkaenkoder == 1) { knopkaenkoder = 2; }
  if (digitalRead(9) == LOW) { knopkaenkoder = 0; }


  if (digitalRead(10) == HIGH&&knopkanazad == 1) { knopkanazad = 2; }
  if (digitalRead(10) == LOW) { knopkanazad = 0; }

  
  if(newTemp>menus[12].value)
  {
    kotelHisteresis = true;
  }
  if(newTemp<menus[12].value-menus[14].value)
  {
    kotelHisteresis = false;
  }
  
  if(newTemp>menus[17].value)
  {
    rozetkaHisteresis= true;
  }
  if(newTemp<menus[17].value-menus[19].value)
  {
    rozetkaHisteresis = false;
  }
  
  
  


//якщо нічний режим закынчився
  if (menus[3].value == 0) 
  {
    //логіка котла

    //по температурі
    if(menus[11].value==0)
    {
      if(kotelHisteresis == false) //якщо гыстерезыс виключився то включаэмо котел
      {
        digitalWrite(4, HIGH);
      }
      else //якщо включився то виключаэмо котел
      {
        digitalWrite(4, LOW);
      }
    }
    //по годинам
    else if(menus[11].value==1)
    {
      if(robochiGodyny[0][t.hour] == true) //якщо годинний біт одиниця то включаємо
      {
        digitalWrite(4, HIGH);
      }
      else if(robochiGodyny[0][t.hour] == false)
      {
        digitalWrite(4, LOW);
      }
    }
    //digitalWrite(5, HIGH); 

    //логіка резетки
    //по температурі
    if(menus[16].value==0)
    {
      if(rozetkaHisteresis == false) //якщо гыстерезыс виключився то включаэмо котел
      {
        digitalWrite(5, HIGH);
      }
      else //якщо включився то виключаэмо котел
      {
        digitalWrite(5, LOW);
      }
    }
    //по годинам
    else if(menus[16].value==1)
    {
      if(robochiGodyny[1][t.hour] == true) //якщо годинний біт одиниця то включаємо
      {
        digitalWrite(5, HIGH);
      }
      else if(robochiGodyny[1][t.hour] == false)
      {
        digitalWrite(5, LOW);
      }
    }
  }
  else { digitalWrite(5, LOW); digitalWrite(4, LOW);}
}

bool isParamEditMode = false;                            // Флаг режима редактирования параметра
int tmpValue = 0;                                // Временная переменная для хранения изменяемого параметра
int chasTmpValue[2];
int CTVIndex = 0;
bool kotelChyRozetka;
void setActualMenu(int v, int h) {
  if (v != 0) {                                      // Двигаемся по вертикали
    if (v == -1) 
    {                                        // Команда ВВЕРХ (отмена)
      if (isParamEditMode) {                              // Если параметр в режиме редактирования, то отменяем изменения
        isParamEditMode = false;
        CTVIndex = 0;
        lcd.noCursor();
        lcd.noBlink();
      }
      else {                                              // Если пункт меню не в режиме редактирования, перемещаемся к родителю
        if (menus[actualIndex].parentid > -1) {            // Если есть куда перемещаться вверх (ParentID>0)
          lcd.clear();
          actualIndex = getMenuIndexByID(menus[actualIndex].parentid);
        }
      }
    }
    else {      
      // Если команда ВНИЗ - входа/редактирования
      if (menus[actualIndex].Param == 3 && !isParamEditMode) 
      { // Если не в режиме редактирования, то ...
        isParamEditMode = true;                           // Переходим в режим редактирования параметра
        tmpValue = menus[actualIndex].value;              // Временной переменной присваиваем актуальное значение параметра
      }
      else if (menus[actualIndex].Param == 4 && !isParamEditMode) 
      { // Если не в режиме редактирования, то ...
        isParamEditMode = true;                           // Переходим в режим редактирования параметра
        tmpValue = 0;              // Временной переменной присваиваем актуальное значение параметра
      }
      else if (menus[actualIndex].Param==1 && !isParamEditMode) 
      { // Если не в режиме редактирования, то ...
        isParamEditMode = true;                           // Переходим в режим редактирования параметра
        tmpValue = menus[actualIndex].value;              // Временной переменной присваиваем актуальное значение параметра
      }
      else if (menus[actualIndex].Param == 2 && !isParamEditMode) 
      { // Если не в режиме редактирования, то ...
        isParamEditMode = true; 
        CTVIndex = 0;
        if (menus[actualIndex].id == 8) {
          chasTmpValue[0] = t.hour;
          chasTmpValue[1] = t.min;
          chasTmpValue[2] = t.sec;
        }
        if (menus[actualIndex].id == 9) {
          chasTmpValue[0] = t.date;
          chasTmpValue[1] = t.mon;
          chasTmpValue[2] = t.year;
        }
 
      }
      else if (menus[actualIndex].Param==1 && isParamEditMode) 
      { // Если в режиме редактирования
        menus[actualIndex].value = tmpValue;              // Сохраняем заданное значение
        ic_eeprom.write_char(actualIndex,tmpValue);
        isParamEditMode = false;                          // И выходим из режима редактирования
      }
      else if (menus[actualIndex].Param == 3 && isParamEditMode) 
      { // Если в режиме редактирования
        menus[actualIndex].value = tmpValue; 
        ic_eeprom.write_char(actualIndex,tmpValue);
        //rtc.writeProtect(false);
        //rtc.setDOW(days[tmpValue]);
        //rtc.writeProtect(true);
        isParamEditMode = false;                          // И выходим из режима редактирования
      }
      else if (menus[actualIndex].Param==4 && isParamEditMode) 
      { // Если в режиме редактирования
        
        //menus[actualIndex].value = tmpValue;              // Сохраняем заданное значение
        //isParamEditMode = false;                          // И выходим из режима редактирования
        //lcd.noCursor();
        //lcd.noBlink();
        if(robochiGodyny[kotelChyRozetka][tmpValue]==false)
        {
          robochiGodyny[kotelChyRozetka][tmpValue]=true;
          saveBitToMemory(true, kotelChyRozetka, tmpValue);
        }
        else
        {
          robochiGodyny[kotelChyRozetka][tmpValue]=false;
          saveBitToMemory(false, kotelChyRozetka, tmpValue);
        }
        
      }
      else if (menus[actualIndex].Param == 2 && isParamEditMode) 
      { // Если в режиме редактирования
        
        if (CTVIndex == 2) 
        {
          CTVIndex = 0;
          //rtc.writeProtect(false);
          if (menus[actualIndex].id == 8) {
            rtc.setTime(chasTmpValue[0], chasTmpValue[1], chasTmpValue[2]);
          }
          if (menus[actualIndex].id == 9) {
            rtc.setDate(chasTmpValue[0], chasTmpValue[1], chasTmpValue[2]);
          }
          //rtc.writeProtect(true);
          //menus[actualIndex].value = tmpValue;              // Сохраняем заданное значение
          isParamEditMode = false;                          // И выходим из режима редактирования
        }
        else { CTVIndex++; }
      }
      else 
      {
        bool nochild = true;                              // Флаг, есть ли дочерние элементы
        for (int i = 0; i < menuArraySize; i++) 
        {
          if (menus[i].parentid == menus[actualIndex].id) 
          {
            actualIndex = i;                              // Если есть, делаем первый попавшийся актуальным элементом
            nochild = false;                              // Потомки есть
            break;                                        // Выходим из for
          }
        }
        if (nochild) {                                    // Если же потомков нет, воспринимаем как команду
          Serial.println("Executing command...");         // И здесь обрабатываем по своему усмотрению
        }
      }
    }
  }

  
  if (h != 0 && !isParamEditMode) // Если горизонтальная навигация и в не режиме редактирования параметра
  { 
    actualIndex = getNearMenuIndexByID(menus[actualIndex].parentid, menus[actualIndex].id, h); //навигация среди потомков одного родителя
    if(menus[actualIndex].id == 15)
    {
      kotelChyRozetka = true;
    }
    if(menus[actualIndex].id == 10)
    {
      kotelChyRozetka = false;
    }
  }

  // Отображаем информацию в Serial
  
  if (isParamEditMode) 
  {
    //if (h != 0) { tmpValue += h; }
    
    /*Serial.println(" > " + menus[actualIndex].name + ": " +
      tmpValue +
      "  min:" + menus[actualIndex]._min +
      ", max:" + menus[actualIndex]._max);*/

    //if (CTVIndex > 2) { CTVIndex = 0; }
    
    switch (menus[actualIndex].Param)
    {
    case 1:
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      lcd.print(clear);
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      if (h != 0) { tmpValue += h; }
      if (tmpValue > menus[actualIndex]._max) tmpValue = menus[actualIndex]._min;
      if (tmpValue < menus[actualIndex]._min) tmpValue = menus[actualIndex]._max;
      lcd.print(tmpValue);
      break;
    case 2:
      if (CTVIndex > 2) { CTVIndex = 0; }
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      lcd.print(clear);
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      if (h != 0) { chasTmpValue[CTVIndex] += h; }
      
      if(menus[actualIndex].id == 8)
      {
        if(chasTmpValue[0]>23){chasTmpValue[0] = 0;}
        if(chasTmpValue[0]<0){chasTmpValue[0] = 23;}
        if(chasTmpValue[1]>59){chasTmpValue[1] = 0;}
        if(chasTmpValue[1]<0){chasTmpValue[1] = 59;}
        if(chasTmpValue[2]>59){chasTmpValue[2] = 0;}
        if(chasTmpValue[2]<0){chasTmpValue[2] = 59;}
      }
      if(menus[actualIndex].id == 9)
      {
        if(chasTmpValue[0]>31){chasTmpValue[0] = 0;}
        if(chasTmpValue[0]<0){chasTmpValue[0] = 31;}
        if(chasTmpValue[1]>12){chasTmpValue[1] = 0;}
        if(chasTmpValue[1]<0){chasTmpValue[1] = 12;}
      }
      
      if (chasTmpValue[0] > -1 && chasTmpValue[0] < 10) { lcd.print('0');  lcd.print(chasTmpValue[0]); }
      else { lcd.print(chasTmpValue[0]); }
      
      lcd.print(':');
      if (chasTmpValue[1] >-1 && chasTmpValue[1] < 10) { lcd.print('0');  lcd.print(chasTmpValue[1]); }
      else { lcd.print(chasTmpValue[1]); }
      lcd.print(':');
      lcd.print(chasTmpValue[2]);

      if (chasTmpValue[CTVIndex] > -1 && chasTmpValue[CTVIndex] < 10) { lcd.setCursor(CTVIndex!=2?(CTVIndex * 3) + 1:6, nomer_ryadka_zminy_parametra); }
      if (chasTmpValue[CTVIndex] > 9 || chasTmpValue[CTVIndex] < 0) { lcd.setCursor(CTVIndex * 3, nomer_ryadka_zminy_parametra); }
      lcd.print(chasTmpValue[CTVIndex]);
      break;
      
      case 3:
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      lcd.print(clear);
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      if (h != 0) { tmpValue += h; }
      if (tmpValue > menus[actualIndex]._max) tmpValue = menus[actualIndex]._min;
      if (tmpValue < menus[actualIndex]._min) tmpValue = menus[actualIndex]._max;
      lcd.print(rezymy[tmpValue]);
      break;
      
      case 4:
      int printedStringLength =0;
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      lcd.print(clear);
      lcd.setCursor(0, nomer_ryadka_zminy_parametra);
      if (h != 0) { tmpValue += h; }
      if (tmpValue > menus[actualIndex]._max) tmpValue = menus[actualIndex]._min;
      if (tmpValue < menus[actualIndex]._min) tmpValue = menus[actualIndex]._max;
      
      lcd.blink();
      
      for(int i=tmpValue;i<tmpValue+4;i++)
      {
        if(i<24)
        {
          lcd.print(i);
          if(robochiGodyny[kotelChyRozetka][i]==false){lcd.print("-");}
          else{lcd.print("+");}
          lcd.print(" ");
        }
      }
      
      if(tmpValue<10)
      {
        lcd.setCursor(1, nomer_ryadka_zminy_parametra);
      }
      else
      {
        lcd.setCursor(2, nomer_ryadka_zminy_parametra);
      }
      
      
      break;
    case 5:
      lcd.print(menus[actualIndex].value);
      break;
    }
  }
  else {
    nomer_ryadka = 0;
    //int nomer_ryadka = 0;
    //if (menus[actualIndex].isParam) {
      //Serial.println(menus[actualIndex]._name + ": " + menus[actualIndex].value);
    //}
    //else {

    /*перебираємо всі пункти меню і шукаємо ті в яких батьківський номер сходиться з батьківським номером актуального меню
    і вміодимо його на лсд екран*/
      for (int k = 0; k < 22; k++) 
      {
        if (menus[actualIndex].parentid == menus[k].parentid)
        {
          lcd.setCursor(0, nomer_ryadka);
          switch (menus[k].Param)
          {
          case -1:
            //lcd.clear();
            PochatkovyEkran = true;
            nomer_ryadka = 5;
            break;  
          case 0:
            PochatkovyEkran = false;
            lcd.print(menus[k].name);
            break;
          case 1:
            lcd.print(menus[k].name);
            break;
          case 2:
            lcd.print(menus[k].name);
            break;
          case 3:
            lcd.print(menus[k].name);
            break;
          case 4:
            lcd.print(menus[k].name);
            break;
          case 5:
            lcd.print(menus[k].name);
            break;

          }



          /*
          if (menus[k].Param == 1) { lcd.print(menus[k].name);  } //lcd.print(":"); lcd.print(menus[k].value);
          else { lcd.print(menus[k].name); }
          */
          if (menus[k].id == menus[actualIndex].id&&menus[k].id > 0) { lcd.setCursor(19, nomer_ryadka); lcd.print('<'); nomer_ryadka_zminy_parametra = nomer_ryadka; }
          nomer_ryadka++;
        }
      }
      //очищаємо наступні рядки екрана від старих відображень
      for (int z = nomer_ryadka; z < 4; z++)
      {
        lcd.setCursor(0, z);
        lcd.print(clear);
      }

      Serial.println(menus[actualIndex].name);
    }
  //}
}


/*void setActualMenu(int v, int h) {








}*/


int getMenuIndexByID(int id) 
{        // Функция получения индекса пункта меню по его ID
  for (int i = 0; i < menuArraySize; i++) {
    if (menus[i].id == id)  return i;
  }
  return -1;
}

int getNearMenuIndexByID(int parentid, int id, int side) { // Функция получения индекса пункта меню следующего или предыдущего от актуального
  int prevID = -1;   // Переменная для хранения индекса предыдущего элемента
  int nextID = -1;   // Переменная для хранения индекса следующего элемента
  int actualID = -1;   // Переменная для хранения индекса актуального элемента

  int firstID = -1;   // Переменная для хранения индекса первого элемента
  int lastID = -1;   // Переменная для хранения индекса последнего элемента

  for (int i = 0; i < menuArraySize; i++) {
    if (menus[i].parentid == parentid) {  // Перебираем все элементы с одним родителем
      if (firstID == -1) firstID = i;     // Запоминаем первый элемент списка

      if (menus[i].id == id) {
        actualID = i;                     // Запоминаем актальный элемент списка
      }
      else {
        if (actualID == -1) {             // Если встретился элемент до актуального, делаем его предыдущим
          prevID = i;
        }
        else if (actualID != -1 && nextID == -1) { // Если встретился элемент после актуального, делаем его следующим
          nextID = i;
        }
      }
      lastID = i;                         // Каждый последующий элемент - последний
    }
  }

  if (nextID == -1) nextID = firstID;     // Если следующего элемента нет - по кругу выдаем первый
  if (prevID == -1) prevID = lastID;      // Если предыдущего элемента нет - по кругу выдаем последний

                      //Serial.println("previusindex:" + (String)prevID + "  nextindex:" + (String)nextID);

  if (side == -1) return prevID;         // В зависимости от направления вращения, выдаем нужный индекс
  else return nextID;
  return -1;
}





void A()
{
  if (micros() - lastTurn < pause) return;  // Если с момента последнего изменения состояния не прошло
                        // достаточно времени - выходим из прерывания
  pinAValue = digitalRead(pinA);            // Получаем состояние пинов A и B
  pinBValue = digitalRead(pinB);

  cli();    // Запрещаем обработку прерываний, чтобы не отвлекаться
  if (state == 0 && !pinAValue &&  pinBValue || state == 2 && pinAValue && !pinBValue) {
    state += 1; // Если выполняется условие, наращиваем переменную state
    lastTurn = micros();
  }
  if (state == -1 && !pinAValue && !pinBValue || state == -3 && pinAValue &&  pinBValue) {
    state -= 1; // Если выполняется условие, наращиваем в минус переменную state
    lastTurn = micros();
  }
  setCount(state); // Проверяем не было ли полного шага из 4 изменений сигналов (2 импульсов)
  sei(); // Разрешаем обработку прерываний

  if (pinAValue && pinBValue && state != 0) state = 0; // Если что-то пошло не так, возвращаем статус в исходное состояние
}
void B()
{
  if (micros() - lastTurn < pause) return;
  pinAValue = digitalRead(pinA);
  pinBValue = digitalRead(pinB);

  cli();
  if (state == 1 && !pinAValue && !pinBValue || state == 3 && pinAValue && pinBValue) {
    state += 1; // Если выполняется условие, наращиваем переменную state
    lastTurn = micros();
  }
  if (state == 0 && pinAValue && !pinBValue || state == -2 && !pinAValue && pinBValue) {
    state -= 1; // Если выполняется условие, наращиваем в минус переменную state
    lastTurn = micros();
  }
  setCount(state); // Проверяем не было ли полного шага из 4 изменений сигналов (2 импульсов)
  sei();

  if (pinAValue && pinBValue && state != 0) state = 0; // Если что-то пошло не так, возвращаем статус в исходное состояние
}

void setCount(int state) 
{          // Устанавливаем значение счетчика
  if (state == 4 || state == -4) {  // Если переменная state приняла заданное значение приращения

    count += (int)(state / 4);      // Увеличиваем/уменьшаем счетчик
    if (count == 1)   flagNext = true;
    if (count == -1) flagPrev = true;
    count = 0;
    lastTurn = micros();            // Запоминаем последнее изменение
  }
}
