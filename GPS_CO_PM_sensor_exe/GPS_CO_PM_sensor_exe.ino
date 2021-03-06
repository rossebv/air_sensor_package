#include <SPI.h>
#include <Adafruit_GPS.h>
#include <SoftwareSerial.h>
#include <SD.h>

File logfile;
int const smallPM1 = 9;
long const sampleRate = 20;
long measurementCount = 0;
long smallPM1Count = 0;
long priorSampleTime = 0;
double smallPM1percentRunning;

unsigned long duration;
unsigned long starttime;
unsigned long sampletime_ms = 2000;
unsigned long lowpulseoccupancy = 0;
float ratio = 0;
float concentration = 0;

#define chipSelect 10
#define ledPin 3
#define GPSECHO  false
#define LOG_FIXONLY true

SoftwareSerial gps_Serial(8, 7);
Adafruit_GPS GPS(&gps_Serial);


void setup() {
  Serial.begin(115200);
  pinMode (smallPM1, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(10, OUTPUT);

  sdCardInitialization();

  Serial.println(F("Initializing GPS..."));
  GPS.begin(9600);

  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);   // uncomment this line to turn on only the "minimum recommended" data; For logging data, we don't suggest using anything but either RMC only or RMC+GGA to keep the log files at a reasonable size
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);

  GPS.sendCommand(PGCMD_ANTENNA);

  gps_Serial.println(PMTK_Q_RELEASE);
  starttime = millis();//get the current time;
}

int loopCount = 1;

void loop() {
  gps_Serial.listen();
  char c = GPS.read();

  if (GPSECHO) {
     if (c)   Serial.print(c);
  }

  if (GPS.newNMEAreceived()) {
    if (!GPS.parse(GPS.lastNMEA()))  { // this also sets the newNMEAreceived() flag to false
      //Serial.println(F("Failed to parse GPS data"));
      return;
    }
    if (LOG_FIXONLY && !GPS.fix) {
        Serial.println("No Fix");
        return;
    }

    printHeader();

    char *stringptr = GPS.lastNMEA();
    uint8_t stringsize = strlen(stringptr);
    if (stringsize != logfile.write((uint8_t *)stringptr, stringsize))  {
      Serial.println(F("error with writing to SD"));
    }

    logData();
    logPM();
    logfile.println();
    logfile.flush();
  }
}


void sdCardInitialization(void)
{
  // SD Card Init
  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD Card init. failed!"));
    while(1);
  } else {
  Serial.println(F("SD Card init successful!"));
  }

  // File Init
  char filename[15];
  sprintf(filename, "/LOG00.csv");
  for (uint8_t i = 0; i < 100; i++) {
    filename[4] = '0' + i/10;
    filename[5] = '0' + i%10;
    if (! SD.exists(filename)) {
      break;
    }
  }

  Serial.print(F("creating file: "));
  Serial.println(filename);
  logfile = SD.open(filename, FILE_WRITE);
  if( ! logfile ) {
    Serial.print(F("Couldnt create ")); Serial.println(filename);
    while(1);
  }
  Serial.print(F("Writing to ")); Serial.println(filename);
}


void logData() {
  logfile.print(F("\n"));
  logfile.print(GPS.month, DEC); logfile.print(F("/"));
  logfile.print(GPS.day, DEC); logfile.print(F("/"));
  logfile.print(F("20"));
  logfile.print(GPS.year, DEC);
  logfile.print(F(","));
  logfile.print(GPS.hour, DEC); logfile.print(F(":"));
  logfile.print(GPS.minute, DEC); logfile.print(F(":"));
  logfile.print(GPS.seconds, DEC); logfile.print(F(":"));
  logfile.print(GPS.milliseconds);
  logfile.print(F(","));
  logfile.print(F("fix_")); logfile.print((int)GPS.fix);
  logfile.print(F(","));
  logfile.print(F("qual_")); logfile.print((int)GPS.fixquality);
  logfile.print(F(","));
  if (GPS.fix) {
    logfile.print(gpsConverter(GPS.latitude), 6); logfile.print(F(",")); logfile.print(GPS.lat);
    logfile.print(F(","));
    logfile.print(gpsConverter(GPS.longitude), 6); logfile.print(F(",")); logfile.print(GPS.lon);
    logfile.print(F(",")); logfile.print(GPS.altitude);
    logfile.print(F(",")); logfile.print(GPS.speed);
    logfile.print(F(",")); logfile.print((int)GPS.satellites);
    logfile.print(F(","));
    logCO();
    logfile.print(F(","));
  }
}

float gpsConverter(float gps)
{
  int gps_min = (int)(gps/100);
  float gps_sec = fmod(gps, 100) / 60;
  float gps_dec = gps_min + gps_sec;
  return gps_dec;
}


void logCO(void)
{
  int reading = analogRead(A0);
  logfile.print(reading);
  analogWrite(ledPin, reading);
//  logfile.println();
  delay(10);
}

void logPM(void)
{
  duration = pulseIn(smallPM1, LOW);
  lowpulseoccupancy = lowpulseoccupancy+duration;

  if ((millis()-starttime) >= sampletime_ms)//if the sampel time = = 30s
  {
    ratio = lowpulseoccupancy/(sampletime_ms*10.0);  // Integer percentage 0=&gt;100
    concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62; // using spec sheet curve
    logfile.print(concentration);
    logfile.print("_pcs/0.01cf");
    logfile.print(F(","));
    logfile.print(pm25pcs2ugm3(concentration));
    logfile.print("_ugm/0.01cf");
    lowpulseoccupancy = 0;
    starttime = millis();
  }
}

float pm25pcs2ugm3 (float concentration_pcs)
{
  double pi = 3.14159;
  double density = 1.65 * pow (10, 12);
  double r25 = 0.44 * pow (10, -6);
  double vol25 = (4/3) * pi * pow (r25, 3);
  double mass25 = density * vol25;
  double K = 3531.5;

  return (concentration_pcs) * K * mass25;
}

void printHeader() {
  Serial.print(F("##### Sample "));
  Serial.print(loopCount++);
  Serial.print(F(" "));
  Serial.print(millis());
  Serial.println(F(" ########################################"));
}

uint8_t parseHex(char c) {
  if (c < '0')
    return 0;
  if (c <= '9')
    return c - '0';
  if (c < 'A')
    return 0;
  if (c <= 'F')
    return (c - 'A')+10;
}
