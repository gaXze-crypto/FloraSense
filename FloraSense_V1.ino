/*
  FloraSense V1 — Giving Plants a Voice
  Board: ELEGOO / Arduino Mega 2560
  Libraries: IRremote (Armin Joachimsmeyer), LedControl (Eberhard Fahle),
             DHT sensor library (Adafruit) + Adafruit Unified Sensor.

  All remote keys use NEC address 0x00, verified from the ELEGOO 21-key remote.
  Passive buzzer uses cooperative software square-wave output, NOT tone().
  This is a single, non-persistent prototype: no EEPROM, RTC or app required.
*/
#include <IRremote.hpp>
#include <LedControl.h>
#include <DHT.h>

// Define custom data types before any functions (Arduino auto-prototype safety).
struct Note { uint16_t hz, ms; };
enum Screen : uint8_t { HOME, MAIN_MENU, MOOD_VIEW, ENV_VIEW, SOUND_VIEW, GAME_MENU, SNAKE_GAME, SIMON_GAME, REACTION_GAME };
enum Mood : uint8_t { HAPPY, HOT, COLD, DRY_AIR, DARK, LOW_WATER, DISTRESS, CRYING };
enum Effect : uint8_t { NO_EFFECT, WAKE_EFFECT, SLEEP_EFFECT, HELLO_EFFECT, REWARD_EFFECT };
enum SimonPhase : uint8_t { SIMON_SHOW, SIMON_INPUT, SIMON_RESULT };
enum ReactionPhase : uint8_t { REACTION_WAIT, REACTION_GO, REACTION_RESULT };

// Explicit prototypes for functions using custom types (Arduino .ino preprocessor).
void playMelody(const Note* melody, uint8_t length);
void moodSound(Mood mood);
void image8(const byte image[8]);
void face8(const byte face[8], bool tears, bool blink);
void handleKey(uint8_t key);

// ------- Pin assignments -------
constexpr uint8_t DHT_PIN=2, BUZZER_PIN=8, RED_PIN=9, GREEN_PIN=10, BLUE_PIN=11;
constexpr uint8_t MATRIX_DIN=22, MATRIX_CS=23, MATRIX_CLK=24;
constexpr uint8_t ULTRA_TRIG=26, ULTRA_ECHO=27, IR_PIN=28;
constexpr uint8_t WATER_PIN=A0, LIGHT_PIN=A1;
#define DHTTYPE DHT11
DHT dht(DHT_PIN,DHTTYPE);
LedControl matrix(MATRIX_DIN,MATRIX_CLK,MATRIX_CS,1);

// ------- Calibrate these for your plant and sensor -------
constexpr float HOT_C=28.0, COLD_C=18.0, DRY_RH=40.0;
constexpr int DARK_RAW=600;         // Previously measured: covered ~40, room ~730
constexpr int WATER_EMPTY_RAW=0;   // Measure analog value with probe OUT of water
constexpr int WATER_FULL_RAW=550;  // Measure analog value at desired full level
constexpr int WATER_LOW_PERCENT=25;
constexpr bool WATER_ENABLED=true; // Set false until water sensor is connected
constexpr unsigned long SENSOR_MS=2200, ULTRA_MS=250;
constexpr unsigned long CRY_AFTER_MS=180000, HELLO_COOLDOWN_MS=30000;
constexpr unsigned long REMOTE_SAME_KEY_GAP_MS=220;

// Remote hex codes (from actual serial diagnostic; 8 is 0x52)
constexpr uint8_t K_ON=0x45,K_VOLUP=0x46,K_BACK=0x47;
constexpr uint8_t K_LEFT=0x44,K_OK=0x40,K_RIGHT=0x43;
constexpr uint8_t K_DOWN=0x07,K_VOLDOWN=0x15,K_UP=0x09;
constexpr uint8_t K_ZERO=0x16,K_EQ=0x19,K_REPEAT=0x0D;
constexpr uint8_t K_ONE=0x0C,K_TWO=0x18,K_THREE=0x5E;

// ------- Matrix bitmaps (original face designs preserved) -------
const byte FACE_HAPPY[8]={B00000000,B01100110,B01100110,B00000000,B00000000,B01000010,B00111100,B00000000};
const byte FACE_HAPPY_OPEN[8]={B00000000,B01100110,B01100110,B00000000,B00111100,B01000010,B01000010,B00111100};
const byte FACE_SAD[8]={B00000000,B01100110,B01100110,B00000000,B00000000,B00111100,B01000010,B00000000};
const byte FACE_SAD_OPEN[8]={B00000000,B01100110,B01100110,B00000000,B00111100,B01000010,B01000010,B00000000};
const byte FACE_SURPRISE[8]={B00000000,B01100110,B01100110,B00000000,B00011000,B00100100,B00100100,B00011000};
const byte FACE_EXCITED[8]={B00000000,B01000010,B00100100,B00000000,B00000000,B01000010,B00111100,B00000000};
const byte FACE_EXCITED_OPEN[8]={B00000000,B01000010,B00100100,B00000000,B00111100,B01000010,B01000010,B00111100};
const byte FACE_SLEEP[8]={B00000000,B00000000,B01100110,B00000000,B00000000,B00111100,B00000000,B00000000};
const byte ICON_HEART[8]={B00000000,B01100110,B11111111,B11111111,B01111110,B00111100,B00011000,B00000000};
const byte ICON_ENV[8]={B00011000,B00100100,B00100100,B00110100,B00110100,B01111110,B01111110,B00111100};
const byte ICON_GAME[8]={B00000000,B00000000,B00111100,B01111110,B11011011,B11111111,B11000011,B01100110};
const byte ICON_SOUND[8]={B00000000,B00010000,B00110010,B11110101,B11110101,B00110010,B00010000,B00000000};
const byte ICON_SNAKE[8]={B00000000,B00000000,B01111100,B01000100,B01011100,B00010000,B00011100,B00000100};
const byte ICON_SIMON[8]={B00000000,B00100100,B01100110,B00011000,B00011000,B01100110,B00100100,B00000000};
const byte ICON_REACT[8]={B00001100,B00011000,B00110000,B01111110,B00001100,B00011000,B00110000,B00000000};
const byte ICON_ARROW[4][8]={
 {B00011000,B00111100,B01011010,B00011000,B00011000,B00011000,B00011000,B00000000}, // UP
 {B00011000,B00011000,B00011000,B00011000,B00011000,B01011010,B00111100,B00011000}, // DOWN
 {B00000000,B00010000,B00110000,B01111110,B00110000,B00010000,B00000000,B00000000}, // LEFT
 {B00000000,B00001000,B00001100,B01111110,B00001100,B00001000,B00000000,B00000000}  // RIGHT
};
const uint8_t NUM3X5[10][5]={
 {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},
 {7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}
};

// ------- System state -------
bool plantOn=false, muted=false, nightMode=false, dhtValid=false, haveSensorSample=false;
Screen screen=HOME;
Effect effect=NO_EFFECT;
unsigned long effectStart=0, lastRemoteAt=0;
uint8_t lastRemoteKey=0;
uint8_t menuIndex=0, gameChoice=0, envPage=0;
float tempC=0, humidity=0, distanceCM=-1;
int lightRaw=0, waterRaw=0, waterPct=0, lightPct=0, healthScore=0;
uint8_t warningMask=0, oldWarningMask=0, warningCount=0, severity=1;
Mood primaryMood=HAPPY;
bool crying=false;
unsigned long problemSince=0, lastWarningAt=0, lastSensorAt=0, lastUltraAt=0;
unsigned long lastColorAt=0, lastBlinkAt=0, lastFrameAt=0;
uint8_t colorIndex=0;
bool visitorNear=false, greetedBefore=false;
unsigned long lastGreetingAt=0;

// ------- Software buzzer (no tone(), no delay()) -------
const Note S_HAPPY[]={{1000,150},{0,80},{1500,200}};
const Note S_HELLO[]={{900,120},{0,70},{1200,150},{0,70},{1600,220}};
const Note S_HOT[]={{1800,120},{0,100},{2000,180}};
const Note S_COLD[]={{400,350},{0,130},{300,450}};
const Note S_DRY[]={{750,200},{0,150},{600,250},{0,150},{750,300}};
const Note S_DARK[]={{900,200},{0,100},{700,220},{0,100},{500,350}};
const Note S_WATER[]={{850,110},{0,130},{850,110},{0,130},{480,350}};
const Note S_DISTRESS[]={{1500,120},{0,80},{1000,120},{0,80},{1500,120},{0,80},{600,350}};
const Note S_CRY[]={{850,100},{950,100},{1050,140},{950,120},{750,200},{550,350},{0,220},{900,100},{1100,130},{1250,160},{1050,130},{850,150},{650,200},{450,400}};
const Note S_WIN[]={{900,100},{0,50},{1200,100},{0,50},{1800,250}};
const Note S_LOSE[]={{600,200},{0,90},{360,290}};
#define PLAY(x) playMelody(x,sizeof(x)/sizeof((x)[0]))
const Note* notes=nullptr;
uint8_t noteCount=0, noteIndex=0;
bool playing=false, speaking=false, buzzerHigh=false;
unsigned long noteAtMs=0, toggleAtUs=0, halfCycleUs=0;

void stopSound(){ playing=false; speaking=false; buzzerHigh=false; digitalWrite(BUZZER_PIN,LOW); }
void beginNote(){
  noteAtMs=millis(); toggleAtUs=micros(); buzzerHigh=false; digitalWrite(BUZZER_PIN,LOW);
  uint16_t hz=notes[noteIndex].hz; halfCycleUs=hz?500000UL/hz:0; speaking=(hz!=0);
}
void playMelody(const Note* melody,uint8_t length){
  stopSound(); if(muted||!plantOn||!length) return;
  notes=melody; noteCount=length; noteIndex=0; playing=true; beginNote();
}
void soundTick(){
  if(!playing) return;
  if(millis()-noteAtMs>=notes[noteIndex].ms){
    ++noteIndex; if(noteIndex>=noteCount){ stopSound(); return; } beginNote();
  }
  if(halfCycleUs && (unsigned long)(micros()-toggleAtUs)>=halfCycleUs){
    toggleAtUs=micros(); buzzerHigh=!buzzerHigh; digitalWrite(BUZZER_PIN,buzzerHigh?HIGH:LOW);
  }
}
void moodSound(Mood mood){
  switch(mood){
    case HAPPY: PLAY(S_HAPPY); break;
    case HOT: PLAY(S_HOT); break;
    case COLD: PLAY(S_COLD); break;
    case DRY_AIR: PLAY(S_DRY); break;
    case DARK: PLAY(S_DARK); break;
    case LOW_WATER: PLAY(S_WATER); break;
    case DISTRESS: PLAY(S_DISTRESS); break;
    case CRYING: PLAY(S_CRY); break;
  }
}

// ------- LED / matrix helpers -------
void rgb(uint8_t r,uint8_t g,uint8_t b){ analogWrite(RED_PIN,r); analogWrite(GREEN_PIN,g); analogWrite(BLUE_PIN,b); }
void image8(const byte image[8]){ for(uint8_t y=0;y<8;y++) matrix.setRow(0,y,image[y]); }
void pixel(uint8_t x,uint8_t y,bool on=true){ if(x<8&&y<8) matrix.setLed(0,y,x,on); }
void number2(uint8_t value){
  value=constrain(value,0,99); matrix.clearDisplay(0);
  uint8_t tens=value/10, ones=value%10;
  for(uint8_t y=0;y<5;y++) for(uint8_t x=0;x<3;x++){
    pixel(0+x,1+y,(NUM3X5[tens][y]>>(2-x))&1);
    pixel(4+x,1+y,(NUM3X5[ones][y]>>(2-x))&1);
  }
}
void face8(const byte face[8],bool tears=false,bool blink=false){
  uint8_t t=(millis()/250)%4;
  for(uint8_t y=0;y<8;y++){
    byte row=face[y];
    if(blink&&y==1) row=0;
    if(blink&&y==2) row=B01100110;
    if(tears && y==3+t) row|=B10000001;
    matrix.setRow(0,y,row);
  }
}
void moodColor(Mood m){
  switch(m){
    case HAPPY: rgb(0,255,0);break;
    case HOT: rgb(255,0,0);break;
    case COLD: rgb(0,0,255);break;
    case DRY_AIR: rgb(255,80,0);break;
    case DARK: rgb(128,0,180);break;
    case LOW_WATER: rgb(0,150,255);break;
    case DISTRESS: rgb(255,0,0);break;
    case CRYING: rgb(120,0,255);break;
  }
}

// ------- Environmental readings -------
int scalePct(int raw,int low,int high){
  if(low==high) return 0;
  return constrain((long)(raw-low)*100L/(high-low),0L,100L);
}
void readSensors(){
  lightRaw=analogRead(LIGHT_PIN);
  waterRaw=WATER_ENABLED?analogRead(WATER_PIN):0;
  waterPct=scalePct(waterRaw,WATER_EMPTY_RAW,WATER_FULL_RAW);
  lightPct=scalePct(lightRaw,40,995);
  float t=dht.readTemperature(), h=dht.readHumidity();
  if(!isnan(t)&&!isnan(h)){ tempC=t;humidity=h;dhtValid=true; }
  else {dhtValid=false; }
  haveSensorSample=true;
  Serial.print(F("TempC:")); Serial.print(dhtValid?tempC:-1);
  Serial.print(F(" Humidity:")); Serial.print(dhtValid?humidity:-1);
  Serial.print(F(" LightRaw:")); Serial.print(lightRaw);
  Serial.print(F(" WaterRaw:")); Serial.print(waterRaw);
  Serial.print(F(" WaterPct:")); Serial.print(waterPct);
  Serial.print(F(" DistanceCm:")); Serial.println(distanceCM);
}

bool isGame(){ return screen==SNAKE_GAME||screen==SIMON_GAME||screen==REACTION_GAME; }
void computeMood(){
  if(!dhtValid){ warningMask=0; healthScore=0; return; }
  uint8_t mask=0; severity=1; warningCount=0;
  if(tempC>HOT_C){mask|=1;severity=max(severity,(uint8_t)(tempC>=33?3:tempC>=30?2:1));}
  else if(tempC<COLD_C){mask|=2;severity=max(severity,(uint8_t)(tempC<=13?3:tempC<=16?2:1));}
  if(humidity<DRY_RH){mask|=4;severity=max(severity,(uint8_t)(humidity<25?3:humidity<32?2:1));}
  if(!nightMode&&lightRaw<DARK_RAW){mask|=8;severity=max(severity,(uint8_t)(lightRaw<250?3:lightRaw<400?2:1));}
  if(WATER_ENABLED && waterPct<WATER_LOW_PERCENT){mask|=16;severity=max(severity,(uint8_t)(waterPct<10?3:2));}
  for(uint8_t b=0;b<5;b++) if(mask&(1<<b)) warningCount++;
  if(warningCount>=2) severity=min((uint8_t)3,(uint8_t)(severity+1));
  // Score is illustrative, not a calibrated horticultural measurement.
  healthScore=constrain(100-(int)warningCount*18-(severity>=3?10:0),0,100);
  uint8_t previous=warningMask;
  warningMask=mask;
  if(mask==0){
    bool recovered=(previous!=0); crying=false; primaryMood=HAPPY;
    if(recovered&&plantOn&&!isGame()&&effect==NO_EFFECT){effect=REWARD_EFFECT; effectStart=millis(); PLAY(S_WIN);}
    oldWarningMask=0; return;
  }
  if(previous==0) problemSince=millis();
  if(!crying && millis()-problemSince>=CRY_AFTER_MS) crying=true;
  if(warningCount>1) primaryMood=DISTRESS;
  else if(mask&1) primaryMood=HOT;
  else if(mask&2) primaryMood=COLD;
  else if(mask&4) primaryMood=DRY_AIR;
  else if(mask&8) primaryMood=DARK;
  else primaryMood=LOW_WATER;
  if(previous!=mask && plantOn && !isGame() && effect==NO_EFFECT){
    moodSound(primaryMood); lastWarningAt=millis();
  }
  oldWarningMask=mask;
}
void warningsTick(){
  if(!plantOn||isGame()||effect!=NO_EFFECT||!dhtValid||!warningMask) return;
  unsigned long period=crying?5000UL:warningCount>1||severity>=3?10000UL:severity==2?30000UL:60000UL;
  if(millis()-lastWarningAt>=period){ moodSound(crying?CRYING:primaryMood);lastWarningAt=millis(); }
}
void distanceTick(){
  if(millis()-lastUltraAt<ULTRA_MS) return;
  lastUltraAt=millis();
  // pulseIn can block up to 12ms. Avoid while making software buzzer notes.
  if(playing||isGame()||effect!=NO_EFFECT) return;
  digitalWrite(ULTRA_TRIG,LOW);delayMicroseconds(2);
  digitalWrite(ULTRA_TRIG,HIGH);delayMicroseconds(10);digitalWrite(ULTRA_TRIG,LOW);
  unsigned long us=pulseIn(ULTRA_ECHO,HIGH,12000UL);
  distanceCM=us?us*0.0343f/2.0f:-1;
  if(distanceCM<0||distanceCM>30){visitorNear=false;return;}
  if(distanceCM>0&&distanceCM<15&&!visitorNear){
    visitorNear=true;
    if(plantOn&&screen==HOME&&(!greetedBefore||millis()-lastGreetingAt>=HELLO_COOLDOWN_MS)){
      greetedBefore=true;lastGreetingAt=millis();effect=HELLO_EFFECT;effectStart=millis(); PLAY(S_HELLO);
      Serial.println(F("Visitor: hello!"));
    }
  }
}

// ------- Menu / screens -------
void showMenu(){
  switch(menuIndex){case 0:image8(ICON_HEART);break;case 1:image8(ICON_ENV);break;
    case 2:image8(ICON_GAME);break;default:image8(ICON_SOUND);break;}
  Serial.print(F("Menu item: "));Serial.println(menuIndex);
}
void showGameMenu(){
  switch(gameChoice){case 0:image8(ICON_SNAKE);Serial.println(F("Game: Snake"));break;
    case 1:image8(ICON_SIMON);Serial.println(F("Game: Simon"));break;
    default:image8(ICON_REACT);Serial.println(F("Game: Reaction"));break;}
}
void showEnvironment(){
  if(!haveSensorSample){image8(ICON_ENV);return;}
  switch(envPage){
    case 0: number2(dhtValid?constrain((int)round(tempC),0,99):0);Serial.print(F("Temperature C: "));Serial.println(dhtValid?tempC:-1);break;
    case 1: number2(dhtValid?constrain((int)round(humidity),0,99):0);Serial.print(F("Humidity %: "));Serial.println(dhtValid?humidity:-1);break;
    case 2: number2(lightPct);Serial.print(F("Light % relative: "));Serial.println(lightPct);break;
    case 3: number2(waterPct);Serial.print(F("Water level %: "));Serial.println(waterPct);break;
    default: number2(healthScore);Serial.print(F("Health score /100: "));Serial.println(healthScore);break;
  }
}
void goHome(){screen=HOME;effect=NO_EFFECT;Serial.println(F("Home"));}
void openMenuItem(){
  switch(menuIndex){
    case 0:screen=MOOD_VIEW;moodSound(crying?CRYING:primaryMood);Serial.println(F("Mood check"));break;
    case 1:screen=ENV_VIEW;envPage=0;showEnvironment();break;
    case 2:screen=GAME_MENU;gameChoice=0;showGameMenu();break;
    default:screen=SOUND_VIEW;image8(ICON_SOUND);Serial.println(muted?F("Muted"):F("Sound enabled"));break;
  }
}

// ------- Snake -------
// 8x8 grid. UP/DOWN/LEFT/RIGHT steer. OK pause/resume. BACK exits.
const uint8_t SNAKE_MAX=64;
uint8_t snakeX[SNAKE_MAX],snakeY[SNAKE_MAX],snakeLen=3,snakeDir=3;
uint8_t foodX=6,foodY=5;
uint8_t snakeScore=0;
bool snakePaused=false,snakeFinished=false;
unsigned long snakeStepAt=0,gameResultAt=0;
int8_t dx[4]={0,0,-1,1},dy[4]={-1,1,0,0};
bool snakeContains(uint8_t x,uint8_t y){for(uint8_t i=0;i<snakeLen;i++)if(snakeX[i]==x&&snakeY[i]==y)return true;return false;}
void snakeDraw(){
  matrix.clearDisplay(0);
  for(uint8_t i=0;i<snakeLen;i++) pixel(snakeX[i],snakeY[i]);
  if(!snakeFinished) pixel(foodX,foodY);
}
void snakeFood(){
  if(snakeLen>=64)return;
  for(uint8_t n=0;n<200;n++){
    uint8_t x=random(8),y=random(8);
    if(!snakeContains(x,y)){foodX=x;foodY=y;return;}
  }
  for(uint8_t y=0;y<8;y++)for(uint8_t x=0;x<8;x++)if(!snakeContains(x,y)){foodX=x;foodY=y;return;}
}
void snakeStart(){
  screen=SNAKE_GAME;effect=NO_EFFECT;snakeLen=3;snakeDir=3;snakeScore=0;
  snakeX[0]=3;snakeY[0]=4;snakeX[1]=2;snakeY[1]=4;snakeX[2]=1;snakeY[2]=4;
  snakePaused=false;snakeFinished=false;snakeStepAt=millis();snakeFood();snakeDraw();
  Serial.println(F("Snake: arrows steer, OK pause, FUNC/STOP exits"));
}
void snakeSteer(uint8_t dir){
  if(snakeFinished)return;
  if((snakeDir==0&&dir==1)||(snakeDir==1&&dir==0)||(snakeDir==2&&dir==3)||(snakeDir==3&&dir==2))return;
  snakeDir=dir;
}
void snakeTick(){
  if(snakeFinished){if(millis()-gameResultAt>1600){snakeStart();snakePaused=true;}return;}
  if(snakePaused||millis()-snakeStepAt<max(170,400-(int)snakeScore*12))return;
  snakeStepAt=millis();
  int nx=(int)snakeX[0]+dx[snakeDir],ny=(int)snakeY[0]+dy[snakeDir];
  bool eating=(nx==foodX&&ny==foodY);
  if(nx<0||nx>=8||ny<0||ny>=8){snakeFinished=true;gameResultAt=millis();PLAY(S_LOSE);number2(snakeScore);return;}
  uint8_t checkLen=eating?snakeLen:snakeLen-1;
  for(uint8_t i=0;i<checkLen;i++)if(snakeX[i]==nx&&snakeY[i]==ny){snakeFinished=true;gameResultAt=millis();PLAY(S_LOSE);number2(snakeScore);return;}
  uint8_t newLen=snakeLen+(eating&&snakeLen<64?1:0);
  for(int i=newLen-1;i>=1;i--){snakeX[i]=snakeX[i-1];snakeY[i]=snakeY[i-1];}
  snakeX[0]=nx;snakeY[0]=ny;snakeLen=newLen;
  if(eating){snakeScore++;PLAY(S_HAPPY);if(snakeLen<64)snakeFood();else{snakeFinished=true;gameResultAt=millis();PLAY(S_WIN);}}
  snakeDraw();
}

// ------- Simon memory -------
// Sequence of directions; watch them, then repeat using arrows.
constexpr uint8_t SIMON_MAX=16;
uint8_t simonSeq[SIMON_MAX];
uint8_t simonLength=1,simonShowIndex=0,simonInputIndex=0;
SimonPhase simonPhase=SIMON_SHOW;
unsigned long simonAt=0;
bool simonLit=false,simonWon=false;
void simonStartRound(){
  simonPhase=SIMON_SHOW;simonShowIndex=0;simonInputIndex=0;simonLit=false;simonAt=millis()+450;
  matrix.clearDisplay(0);Serial.print(F("Simon round: "));Serial.println(simonLength);
}
void simonStart(){
  screen=SIMON_GAME;effect=NO_EFFECT;simonLength=1;simonWon=false;
  for(uint8_t i=0;i<SIMON_MAX;i++)simonSeq[i]=random(4);
  simonStartRound();Serial.println(F("Simon: watch arrows, then repeat them"));
}
void simonInput(uint8_t dir){
  if(simonPhase!=SIMON_INPUT)return;
  if(dir==simonSeq[simonInputIndex]){
    simonInputIndex++;PLAY(S_HAPPY);
    if(simonInputIndex>=simonLength){
      if(simonLength>=SIMON_MAX){simonWon=true;simonPhase=SIMON_RESULT;gameResultAt=millis();PLAY(S_WIN);number2(simonLength);}
      else {simonLength++;simonStartRound();}
    }
  }else{ simonWon=false;simonPhase=SIMON_RESULT;gameResultAt=millis();PLAY(S_LOSE);number2(simonLength-1); }
}
void simonTick(){
  if(simonPhase==SIMON_RESULT){if(millis()-gameResultAt>1800){simonStart();}return;}
  if(simonPhase!=SIMON_SHOW||millis()<simonAt)return;
  if(!simonLit){
    image8(ICON_ARROW[simonSeq[simonShowIndex]]);simonLit=true;simonAt=millis()+420;
  }else{
    matrix.clearDisplay(0);simonLit=false;simonShowIndex++;simonAt=millis()+240;
    if(simonShowIndex>=simonLength){simonPhase=SIMON_INPUT;image8(ICON_SIMON);Serial.println(F("Simon: your turn"));}
  }
}

// ------- Reaction challenge -------
// Wait for the GO icon, then press OK as quickly as possible.
ReactionPhase reactionPhase=REACTION_WAIT;
unsigned long reactionGoAt=0,reactionStartedAt=0;
uint16_t reactionMs=0;
void reactionStart(){
  screen=REACTION_GAME;effect=NO_EFFECT;reactionPhase=REACTION_WAIT;
  reactionGoAt=millis()+random(1600,4500);image8(FACE_SLEEP);
  Serial.println(F("Reaction: wait for lightning, then press OK"));
}
void reactionPress(){
  if(reactionPhase==REACTION_WAIT){reactionPhase=REACTION_RESULT;gameResultAt=millis();number2(0);PLAY(S_LOSE);Serial.println(F("Too early!"));}
  else if(reactionPhase==REACTION_GO){
    reactionMs=constrain(millis()-reactionStartedAt,0UL,9999UL);
    reactionPhase=REACTION_RESULT;gameResultAt=millis();number2(min(99,(int)(reactionMs/10)));
    PLAY(S_WIN);Serial.print(F("Reaction milliseconds: "));Serial.println(reactionMs);
    Serial.println(F("Display shows centiseconds (capped at 99)"));
  }else reactionStart();
}
void reactionTick(){
  if(reactionPhase==REACTION_WAIT&&millis()>=reactionGoAt){reactionPhase=REACTION_GO;reactionStartedAt=millis();image8(ICON_REACT);rgb(0,255,0);}
  if(reactionPhase==REACTION_RESULT&&millis()-gameResultAt>2200)reactionStart();
}

// ------- Remote inputs -------
void powerToggle(){
  if(plantOn){plantOn=false;stopSound();screen=HOME;effect=SLEEP_EFFECT;effectStart=millis();Serial.println(F("FloraSense sleeping"));}
  else{plantOn=true;screen=HOME;effect=WAKE_EFFECT;effectStart=millis();rgb(0,255,0);PLAY(S_HAPPY);Serial.println(F("FloraSense awake"));}
}
void handleKey(uint8_t key){
  if(key==K_ON){powerToggle();return;}
  if(!plantOn)return;
  if(key==K_VOLDOWN){muted=!muted;if(muted)stopSound();Serial.println(muted?F("MUTED"):F("UNMUTED"));return;}
  if(key==K_VOLUP){PLAY(S_HELLO);Serial.println(F("Greeting sound"));return;}
  if(key==K_EQ){nightMode=!nightMode;computeMood();Serial.println(nightMode?F("NIGHT mode: darkness OK"):F("DAY mode"));return;}
  if(key==K_BACK){
    effect=NO_EFFECT;
    if(isGame()) {screen=GAME_MENU;showGameMenu();stopSound();}
    else if(screen==GAME_MENU||screen==MOOD_VIEW||screen==ENV_VIEW||screen==SOUND_VIEW){screen=MAIN_MENU;showMenu();}
    else if(screen==MAIN_MENU)goHome();
    return;
  }
  if(effect==WAKE_EFFECT||effect==HELLO_EFFECT||effect==REWARD_EFFECT) effect=NO_EFFECT;
  uint8_t dir=255;
  if(key==K_UP)dir=0;else if(key==K_DOWN)dir=1;else if(key==K_LEFT)dir=2;else if(key==K_RIGHT)dir=3;
  if(screen==SNAKE_GAME){
    if(dir!=255)snakeSteer(dir);
    else if(key==K_OK){if(snakeFinished)snakeStart();else snakePaused=!snakePaused;Serial.println(snakePaused?F("Snake paused"):F("Snake running"));}
    return;
  }
  if(screen==SIMON_GAME){if(dir!=255)simonInput(dir);else if(key==K_OK&&simonPhase==SIMON_RESULT)simonStart();return;}
  if(screen==REACTION_GAME){if(key==K_OK)reactionPress();return;}
  if(screen==HOME){if(key==K_OK){screen=MAIN_MENU;menuIndex=0;showMenu();}return;}
  if(screen==MAIN_MENU){
    if(dir!=255){menuIndex=(menuIndex+(dir==0||dir==2?3:1))%4;showMenu();}
    else if(key==K_OK)openMenuItem();return;
  }
  if(screen==GAME_MENU){
    if(dir!=255){gameChoice=(gameChoice+(dir==0||dir==2?2:1))%3;showGameMenu();}
    else if(key==K_OK){if(gameChoice==0)snakeStart();else if(gameChoice==1)simonStart();else reactionStart();}
    return;
  }
  if(screen==ENV_VIEW){
    if(dir!=255){envPage=(envPage+(dir==0||dir==2?4:1))%5;showEnvironment();}
    else if(key==K_OK)showEnvironment();return;
  }
  if(screen==MOOD_VIEW && key==K_OK){moodSound(crying?CRYING:primaryMood);return;}
  if(screen==SOUND_VIEW && key==K_OK){muted=!muted;if(muted)stopSound();Serial.println(muted?F("MUTED"):F("UNMUTED"));return;}
}
void remoteTick(){
  if(!IrReceiver.decode())return;
  uint16_t command=IrReceiver.decodedIRData.command;
  uint16_t address=IrReceiver.decodedIRData.address;
  uint8_t flags=IrReceiver.decodedIRData.flags;
  decode_type_t protocol=IrReceiver.decodedIRData.protocol;
  IrReceiver.resume();
  if(protocol!=NEC||address!=0||command==0||command>255||(flags&IRDATA_FLAGS_IS_REPEAT))return;
  unsigned long now=millis();
  if(command==lastRemoteKey&&now-lastRemoteAt<REMOTE_SAME_KEY_GAP_MS)return;
  lastRemoteKey=command;lastRemoteAt=now;
  Serial.print(F("Key: 0x"));Serial.println(command,HEX);
  handleKey((uint8_t)command);
}

// ------- Display animations -------
void frameTick(){
  if(millis()-lastFrameAt<75)return;
  lastFrameAt=millis();
  if(!plantOn&&effect!=SLEEP_EFFECT)return;
  if(effect!=NO_EFFECT){
    unsigned long t=millis()-effectStart;
    if(effect==SLEEP_EFFECT){
      if(t<850){image8(FACE_SLEEP);rgb(0,(uint8_t)(t<350?55:10),0);}else{effect=NO_EFFECT;matrix.clearDisplay(0);rgb(0,0,0);}return;
    }
    if(effect==WAKE_EFFECT){
      if(t<300)image8(FACE_SLEEP);
      else if(t<650)image8(FACE_HAPPY);
      else if(t<1200)image8(speaking?FACE_EXCITED_OPEN:FACE_EXCITED);
      else effect=NO_EFFECT;
      return;
    }
    if(effect==HELLO_EFFECT){
      if(t<450)image8(FACE_SURPRISE);
      else if(t<1800)image8(speaking?FACE_EXCITED_OPEN:FACE_EXCITED);
      else if(t<2700)image8((t/150)%2?FACE_HAPPY:FACE_EXCITED);
      else if(t<3700)image8(FACE_HAPPY);
      else effect=NO_EFFECT;
      return;
    }
    if(effect==REWARD_EFFECT){
      if(t<1300)image8((t/220)%2?ICON_HEART:FACE_EXCITED);
      else effect=NO_EFFECT;
      return;
    }
  }
  if(screen==HOME||screen==MOOD_VIEW){
    bool blink=(millis()-lastBlink<150);
    if(!dhtValid){image8(FACE_SLEEP);return;}
    if(warningMask){face8(speaking?FACE_SAD_OPEN:FACE_SAD,crying,blink);}
    else face8(speaking?FACE_HAPPY_OPEN:FACE_HAPPY,false,blink);
  }
}
void colorTick(){
  if(!plantOn||effect==SLEEP_EFFECT||isGame())return;
  if(effect==WAKE_EFFECT){rgb(0,255,0);return;}
  if(effect==HELLO_EFFECT){rgb((millis()/250)%2?0:255,255,(millis()/250)%2?255:0);return;}
  if(effect==REWARD_EFFECT){rgb(0,255,0);return;}
  if(millis()-lastColorAt<1000)return;
  lastColorAt=millis();
  if(!dhtValid){rgb(0,0,0);return;}
  if(!warningMask){moodColor(HAPPY);return;}
  if(crying && warningCount==1){moodColor(CRYING);return;}
  Mood list[5];uint8_t n=0;
  if(warningMask&1)list[n++]=HOT;
  if(warningMask&2)list[n++]=COLD;
  if(warningMask&4)list[n++]=DRY_AIR;
  if(warningMask&8)list[n++]=DARK;
  if(warningMask&16)list[n++]=LOW_WATER;
  if(n){moodColor(list[colorIndex%n]);colorIndex++;}
}

void setup(){
  Serial.begin(9600);
  pinMode(BUZZER_PIN,OUTPUT);digitalWrite(BUZZER_PIN,LOW);
  pinMode(RED_PIN,OUTPUT);pinMode(GREEN_PIN,OUTPUT);pinMode(BLUE_PIN,OUTPUT);
  pinMode(ULTRA_TRIG,OUTPUT);pinMode(ULTRA_ECHO,INPUT);digitalWrite(ULTRA_TRIG,LOW);
  pinMode(WATER_PIN,INPUT);pinMode(LIGHT_PIN,INPUT);
  dht.begin();matrix.shutdown(0,false);matrix.setIntensity(0,4);matrix.clearDisplay(0);
  IrReceiver.begin(IR_PIN,ENABLE_LED_FEEDBACK);
  rgb(0,0,0);randomSeed(analogRead(A7)+micros());
  unsigned long now=millis();lastSensorAt=now-SENSOR_MS;lastUltraAt=now;lastBlinkAt=now;
  Serial.println(F("FloraSense V1 ready. ON=power, OK=menu, FUNC/STOP=back."));
  Serial.println(F("EQ toggles manual DAY/NIGHT. Games: Snake, Simon, Reaction."));
}
void loop(){
  remoteTick();soundTick();
  if(millis()-lastSensorAt>=SENSOR_MS){lastSensorAt=millis();readSensors();computeMood();}
  distanceTick();warningsTick();
  if(millis()-lastBlinkAt>=3500){lastBlinkAt=millis();}
  if(plantOn){
    if(screen==SNAKE_GAME)snakeTick();
    else if(screen==SIMON_GAME)simonTick();
    else if(screen==REACTION_GAME)reactionTick();
  }
  colorTick();frameTick();
}
