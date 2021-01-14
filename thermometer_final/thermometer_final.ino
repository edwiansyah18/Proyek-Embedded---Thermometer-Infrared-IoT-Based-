// library yang digunakan
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <avr/sleep.h>

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define RX 2                // define pin untuk koneksi melalui esp8266
#define TX 3
#define interruptPin 3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // alokasi define untuk OLED
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

const int buzzer = 6;     // define pin buzzer
double temp_amb;          // define variabel yang digunakan pada program
double temp_obj;
double temp_sum = 0;
double temp_average;
int counter = 0;

// variabel yang digunakan untuk melakukan koneksi ke ThingSpeak (sebagai platform untuk melakukan IoT)
String AP = "ADIVAEWIN";                  // nama//SSID wifi yang digunakan
String PASS = "887358em";                 // password wifi yang digunakan
String API = "OS53VL42UF0XTVS0";          // API KEY khusus yang didapatkan dari ThingSpeak
String HOST = "api.thingspeak.com";
String PORT = "80";
String field = "field1";
int countTrueCommand;
int countTimeCommand; 
boolean found = false; 
int valSensor = 1;
SoftwareSerial esp8266(RX,TX);

// ----------------------------------------------------------------------------------------------------
void setup()
{
  Serial.begin(9600);
  pinMode(buzzer, OUTPUT);  // menjadikan buzzer sebagai output  
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(interruptPin,INPUT_PULLUP);                         // interrupt eksternal melalui pin 3 untuk melakukan sleep
  attachInterrupt(digitalPinToInterrupt(2),checking,RISING);  // interrupt eksternal melalui pin 2

  // set baud rate untuk esp8266 dan mengecek apakah esp berfungsi dengan benar atau tidak
  esp8266.begin(115200);                
  sendCommand("AT",5,"OK");
  sendCommand("AT+CWMODE=1",5,"OK");
  sendCommand("AT+CWJAP=\""+ AP +"\",\""+ PASS +"\"",20,"OK");
  
  mlx.begin();                               //Initialize MLX90614 (sensor suhu infrared)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initialize with the I2C addr 0x3C (oled)
 
  Serial.println("Temperature Sensor MLX90614");

  //set up oled
  display.clearDisplay();
  display.setCursor(25,15);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println(" Thermometer");
  display.setCursor(25,35);
  display.setTextSize(1);
  display.print("Initializing");
  display.display();
  delay(5000);
}

void loop()
{
  // memasukan nilai yang didapat sensor ke variabel
  temp_amb = mlx.readAmbientTempC();
  temp_obj = mlx.readObjectTempC();
 
  //Serial Monitor
  Serial.print("Room Temp = ");
  Serial.println(temp_amb);
  Serial.print("Object temp = ");
  Serial.println(temp_obj);
  
  idling();
  
  if (counter <= 300000){ // jika dalam 5 menit tidak ada interrupt, sistem akan masuk ke mode sleep
    counter++;
    }
    else{
    Going_To_Sleep();
    }
}

// ----------------------------------------------------------------------------------------------------
// idle dari perangkat ini, dimana hanya dilakukan pembacaan suhu saja
void idling(){
  // menampilkan display 'Thermometer' nilai dari variabel sensor
  display.clearDisplay();
  display.setCursor(25,10);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.println(" Temperature");
  display.setCursor(25,30);
  display.setTextSize(2);
  display.print(temp_obj);
  display.print((char)247);
  display.print("C");
  display.display();
  delay(1000);
  }

// fungsi yang akan digunakan jika ada interrupt (pin 2)
void checking(){
  Serial.print("interrupt");
  for (int i = 1; i <= 5; i++) {    // sistem akan melakukan pembacaan suhu sebanyak 5 kali
    idling();
    temp_sum += temp_obj;
     Serial.print("Temp got = ");
     Serial.println(temp_obj);
  }
  temp_average = temp_sum/5;        // lalu di cari rata rata dari 5 suhu yang didapatkan
  setupesp();
  Serial.print("avg Temp = ");
  Serial.println(temp_average);
  if (temp_average >= 37){          // jika suhu yang didapatkan >= 37C, maka akan masuk ke fungsi warning()
    warning();
    }
  else{                             // jika suhu yang didapatkan < 37C, maka akan masuk ke fungsi safe()
    safe();
    }
  temp_sum = 0;                     // reset sum agar bisa digunakan kembali 
  counter = 0;                      // reset counter untuk mode sleep
}

// fungsi untuk memberi peringatan, dengan memberikan display khusus dan buzzer yang menyala
void warning(){
    tone(buzzer, 1000);
    display.clearDisplay();
    display.setCursor(45,10);  
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(temp_average);
    display.print((char)247);
    display.print("C");
    display.setCursor(20,30);
    display.setTextSize(2);
    display.println("Warning!");
    display.display();
    delay(3000);
    noTone(buzzer);
  }

// fungsi untuk memberitahukan bahwa pihak yang dibaca suhunya aman dari suhu tubuh panas
void safe(){
    display.clearDisplay();
    display.setCursor(45,10);  
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.print(temp_average);
    display.print((char)247);
    display.print("C");
    display.setCursor(20,30);
    display.setTextSize(2);
    display.println("S a f e!");
    display.display();
    delay(3000);
  }

// fungsi untuk mengirimkan data esp8266 ke ThingSpeak
void setupesp(){
  valSensor = getSensorData();
  String getData = "GET /update?api_key="+ API +"&"+ field +"="+String(valSensor);
  sendCommand("AT+CIPMUX=1",5,"OK");
  sendCommand("AT+CIPSTART=0,\"TCP\",\""+ HOST +"\","+ PORT,15,"OK");
  sendCommand("AT+CIPSEND=0," +String(getData.length()+4),4,">");
  esp8266.println(getData);delay(1500);countTrueCommand++;
  sendCommand("AT+CIPCLOSE=0",5,"OK");
  }

// fungsi untuk mendefine variabel apa yang dikirim ke ThingSpeak
int getSensorData(){
  return temp_average; // mengembalikan suhu tubuh rata-rata pada setiap interrupt
}

// fungsi untuk mengecek esp8266
void sendCommand(String command, int maxTime, char readReplay[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  while(countTimeCommand < (maxTime*1))
  {
    esp8266.println(command);//at+cipsend
    if(esp8266.find(readReplay))//ok
    {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true)
  {
    Serial.println("OYI");
    countTrueCommand++;
    countTimeCommand = 0;
  }
}

// fungsi untuk masuk ke mode sleep
void Going_To_Sleep(){
    sleep_enable();
    attachInterrupt(1, wakeUp, LOW);
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    delay(1000);
    sleep_cpu();               //activating sleep mode
  }

// fungsi untuk wakeup atau keluar dari mode sleep
void wakeUp(){
  sleep_disable();            //Disable sleep mode
  detachInterrupt(1);         //Removes the interrupt from pin 3;
}
