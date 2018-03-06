#include <Servo.h>
#include <NewPing.h>
#include <FreqCount.h>

#define leftservo 0 //left servo
#define rightservo 1 //right servo
#define tankservo 2 //tank spinning servo
#define ductedfan 3 //ducted fan
#define trig1 4 //sonar front
#define echo1 5
#define trig2 6 //sonar right 1
#define echo2 7
#define trig3 8 //sonar right 2
#define echo3 9 
#define readyled 22 //green LED to indicate sound and other things
#define fireled 23 //red LED to indicate fire
#define modeswitch 12 //switch between sumo & firefighting (not used)
#define mic 13 //microphone
#define fire1 A0 //fire sensor
#define fire2 A1
#define fire3 A2
#define fire4 A3
#define fire5 A4
#define fire6 A5
#define line A6 //line sensor
#define sharp A7 //front IR

#define isFire 5 // fire sensor value indicating fire (1-2)
#define fireTol 5 //aiming tolerance
#define firekeep 30 //keep aiming loop
#define fireoverride 1 //override the room check to put out fire

#define front 7 //turn 90 when meet front
#define frontsharp 600 //sharp sensor front for the above distance in cm
#define happy 19 //keep right distance
#define happytol 2 //tolerance
#define nowall 25 //no wall distance
#define turn90 450 //800

#define isWhite 200 //if it is white
#define basewhitetime 200

#define paninit 1400

#define roomfourtime 700 

byte distance[3]; //distances, 0 = front, 1 = right 1, 2 = right 2

int fireval[6]; //new fire values
int pfireval[6]; //previous fire values for history keeping
int pfirecount = 0; //counter for erasing previous fire values
int servopos = 0; //suggesting the tank servo position 
boolean fireboo = false; //for checking if there is fire

boolean firesearchleft = true; //spinning head

int fanspeed = 0; //controls the fan speed, from 0 - 1000

boolean pwhite = false;
boolean white = false;
long whitetime;
boolean inRoom = false;
boolean meetBase = false;
boolean ignore = false;

boolean visit4 = false;

NewPing sonar[3] = { //sonar instance
  NewPing(trig1,echo1,200),
  NewPing(trig2,echo2,200),
  NewPing(trig3,echo3,200)
};

Servo fan; //ducted fan
Servo left; //left motor
Servo right; //right motor
Servo pan; //top servo

//blink start

void ledblink(int pin){ //blink the LED on the pin
  pinMode(pin, OUTPUT);
  for(int a = 0; a<3; a++){
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
  }
}

//blink stop

//mic start

void waitsound(){ //needs FreqCount instance, waits for 3100hz to 4300hz sound
  unsigned long count;
  while(count<3100 || count >4300){
    if(FreqCount.available()){
      count = FreqCount.read();
    }
  }
}

//mic ends

//sonic starts

void sonicLoop(){ //keep good sonic distances
  for(int a = 0; a < 3; a++){
    int dist = sonar[a].ping_cm();
    while(dist <2 || dist >200){
      delayMicroseconds(50);
      dist = sonar[a].ping_cm();
    }
    distance[a] = dist;
    delayMicroseconds(50);
  }
}

void stsonic(){
  for(int a = 0; a<1000; a++){
    sonicLoop();
    if(distance[0]>front){
      break;
    }
  }
}

//sonic ends

//fire sensor starts

void firesensorLoop(){
  fireval[0] = 1023 - analogRead(fire1); //reading fire values from sensor
  fireval[1] = 1023 - analogRead(fire2);
  fireval[2] = 1023 - analogRead(fire3);
  fireval[3] = 1023 - analogRead(fire4);
  fireval[4] = 1023 - analogRead(fire5);
  fireval[5] = 1023 - analogRead(fire6);
  boolean nonofire = true; //assuming there is no fire
  for(int a = 0; a< 6; a++){ //for each new fire value 
    if(fireval[a]>isFire || pfirecount >firekeep){ //if there is fire or the previous fire should be erased
      pfirecount = 0; //reset counter
      for(int b = 0; b<6; b++){
        pfireval[b] = fireval[b]; //make the previous fire values same as the new fire values
      }
      nonofire = false; //previous is reset
      break;
    }
  }
  if(nonofire){ //counter for erasing pfire
    pfirecount ++;
  }
}

void fireLoop(){
  int temp = 0; //keep the highest fire reading
  int firepos = -1;  //keep the highest fire position
  for(int a = 0; a<6; a++){ //search for the highest reading and position
    if(pfireval[a] > temp){
      temp = pfireval[a];
      firepos = a;
    }
  }
  if(temp >= isFire){ //if there is fire
    digitalWrite(fireled, HIGH); //turn on the fire LED
    switch(firepos){ //switch the position
      case 0: //turn left a lot
      servopos +=100;
      fanspeed = 0;
      break;
      case 1: //turn left a little bit
      servopos +=50;
      fanspeed = 1000;
      break;
      case 2:
      if(pfireval[2]>pfireval[3]+fireTol){ //turn left a tiny bit
        servopos += 30;
      }
      stop();
//      if(inRoom||(pfireval[2]>=fireoverride)){
        fanspeed = 1000; //turn on the fan
//      }
      break;
      case 3: 
      if(pfireval[3]>pfireval[2]+fireTol){ //turn right a tiny bit
        servopos -= 30;
      }
      stop();
//      if(inRoom||(pfireval[3]>=fireoverride)){
        fanspeed = 1000; //turn on the fan
//      }
      break;
      case 4: //turn right a little bit
      servopos -= 50;
      fanspeed = 1000;
      break;
      case 5:
      servopos -= 100; //turn right a lot
      fanspeed = 1000;
      break;
      default:
      break;
      fanspeed = 0;
    }
  }else{
    digitalWrite(fireled, LOW); //turn off the fire led
    servopos = -100;
    fanspeed = 0;
//    if(firesearchleft){ //if this run is searching right (typo)
//      servopos -=100; //go right
//      if(servopos <= -1000){
//        firesearchleft = false; 
//      }
//    }else{ //opposite
//      servopos +=100;
//     if(servopos >= 1000){
//        firesearchleft = true; 
//      }
//    }
  }
  if(servopos > 1000){ //make sure the value is in bound)
    servopos = 1000;
  }
  if(servopos<-1000){
    servopos = -1000;
  }
}

//fire sensor ends

// All sensor loop

void sensorLoop(){
  sonicLoop();
  firesensorLoop();
}

// All sensor loop ends

//Servo

int leftspeed;
int rightspeed;

void perfectright(){
  leftspeed = 100; // 100 71
  rightspeed = 62;
}

void perfectleft(){
  leftspeed = 62; // 71 100
  rightspeed = 100;
}

void fixturn(){
  leftspeed = 100 * (1 - 0.02 * (distance[2] - distance[1]));
  rightspeed = 100 * (1 - 0.02 * (distance[1] - distance[2]));
}

void turnleft90(){
  stop();
  go();
  delay(100);
  leftspeed = -40;
  rightspeed = 40;
  go();
  delay(turn90);
  stop();
  go();
}

void forward(){
  leftspeed = 100;
  rightspeed = 100;
}

void go(){
  if(leftspeed >100){
    leftspeed = 100;
  }
  if(leftspeed <-100){
    leftspeed = -100;
  }
  if(rightspeed >100){
    rightspeed = 100;
  }
  if(rightspeed <-100){
    rightspeed = -100;
  }
  left.writeMicroseconds(1500 - 0.88*leftspeed); //3.95
  right.writeMicroseconds(1500 + rightspeed); //5
  pan.writeMicroseconds(servopos + 1500);
  fan.writeMicroseconds(fanspeed + 1000);
}

void stop(){
  leftspeed = 0;
  rightspeed = 0;
}

//Servo ends

//fire starts

//wall follower

void wallfollow(){
  if(/*distance[0] <= front ||*/ analogRead(A7)>=frontsharp){
    turnleft90();
  }else{
    if(distance[1] >= nowall || distance[2]>= nowall){
      perfectright();
    }else if(distance[1] > happy + happytol){
      if(distance[1] < distance[2] - 2){
        forward();
      }else{
        perfectright();
      }
    }else if(distance[1] < happy - happytol){
      if(distance[1] > distance[2] + 2){
        forward();
      }else{
        perfectleft();
      }
    }else{
      fixturn();
    }
  }
}

//wall follower ends

//room checker

void roomchecker(){
  white = (analogRead(line) < isWhite);
  if(white && !pwhite){
    whitetime = millis();
  }else if(!white && pwhite){
    if(!ignore){
      inRoom = !inRoom;
    }else{
      ignore = false;
    }
    digitalWrite(readyled, inRoom);
  }else if(white && pwhite && (millis() - whitetime > basewhitetime)){
    if(meetBase && (!ignore)){
//      left.detach();
//      right.detach();
//      go();
//      while(true);
    }else{
      meetBase = true;
      ignore = true;
    }
  }
  pwhite = white;
}

//room checker ends

void room4(){
  sensorLoop();
  if(distance[1] <= nowall){
    turnleft90();
  }
  forward();
  go();
  delay(roomfourtime);
  while(true){
    sensorLoop();
    fireLoop();
    wallfollow();
    roomchecker();
    go();
    if(inRoom){
      visit4 = true;
    }
    if(!inRoom && visit4){
      break;
    }
  }
  while(analogRead(sharp) < frontsharp){
    forward();
    go();
  }
  turnleft90();
}

void turnleft90checker(){
  turnleft90();
  stop();
  go();
  while(true);
}

void room4checker(){
  sensorLoop();
  if(distance[1] <= nowall){
    turnleft90();
  }
  forward();
  go();
  delay(roomfourtime);
  stop();
  go();
  while(true);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  left.attach(leftservo);
  right.attach(rightservo);
  pan.attach(tankservo);
  fan.attach(ductedfan);
  fan.writeMicroseconds(1000);
  pan.writeMicroseconds(paninit);
  FreqCount.begin(1000);
  pinMode(readyled, OUTPUT);
  pinMode(fireled, OUTPUT);
  ledblink(readyled);
  waitsound();
  ledblink(readyled);
  room4();
}

void loop() {
  // put your main code here, to run repeatedly:
  sensorLoop();
  wallfollow();
  fireLoop();
  roomchecker();

//  turnleft90checker();
//  room4checker();

  go();
}
