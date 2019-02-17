/*
 * Arduino Pro Miniにて、
 *  バイクの
 *    ・ヘッドライトの自動点灯
 *    ・ウインカーのOn/Off 自動キャンセル
 *  を行う
 *
 *  created 2017/07/02 高橋夏彦
 *  updated 2017/08/26 オートキャンセル Ver-2.0.0
 */


//ヘッドライト関連
const int PIN_ANALOG_INPUT_CDS_SENSOR = 8;                    //CDSからの電圧
const int PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY = 21;            //ヘッドライトのリレー
const int PIN_DIGITAL_INPUT_NEUTRAL = 7;                      //ニュートラルスイッチ


//ウインカー関連
const int PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT_SW = 4;           //ウインカー左スイッチからの信号(濃茶)
const int PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW = 5;         //ウインカーキャンセルスイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW = 6;          //ウインカー右スイッチからの信号(濃緑)
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY = 22;     //ウインカー右のリレー(濃緑)
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY = 23;      //ウインカー左のリレー(濃茶)
enum ETurnSignalState {OFF = 0, ON = 1};                      //ウインカーの状態
ETurnSignalState CurrentTurnSignalState
                                = ETurnSignalState::OFF;

//速度計測関連
const int PIN_INTERRUPT_SPEED_PULSE = 1;                      //回転速度センサーからの割込みピン
const float NUMBER_OF_PULSES_PER_METER = 23.0;                //1mあたりのパルス数(ドライブスプロケットからの検出数)
volatile int PulseCount = 0;                                  //回転速度センサーからのパルス数 
int CurrentSpeed = 0;                                         //現在の車速

//車速変化の状態(通常走行中、徐行中、減速中、ほぼ停止)
enum ESpeedState {NORMAL_RUNNING = 0,
                  SLOW_RUNNING = 1,
                  SLOW_DOWN = 2,
                  ALMOST_STOP = 3};

ESpeedState CurrentSpeedState = ESpeedState::ALMOST_STOP;     //車速変化の現在の状態


/*
 * setup() 
 * 
 */
void setup() {
  pinMode(PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY, OUTPUT);
  pinMode(PIN_DIGITAL_INPUT_NEUTRAL, INPUT);
  
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT_SW, INPUT);
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW, INPUT);
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW, INPUT);  
  pinMode(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, OUTPUT);
  pinMode(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT_SPEED_PULSE), pulseCounter, RISING );
  
  Serial.begin( 9600 );
}


/*
 * loop()
 * 
 */
void loop() {
  
  //ヘッドライトの制御
  headLightControl();

  //ウインカーの制御
  turnSignalControl();

  //車速の計算
  calcMovingSpeed();
  
  //ウインカーオートキャンセル
  turnSignalAutoCancelControl();
  
  //デバッグコード
//  Serial.println( PulseCount );
}


/*
 * headLightControl ヘッドライトの制御
 *   CDSからの電圧をチェックし、一定時間、閾値を上回ったらヘッドライトを点灯する
 *                 〃                           下回ったらヘッドライトを消灯する
 */
void headLightControl() {
  static int prevState = LOW;
  static int currentState = LOW;
  static bool timerStart = false;
  static long timer = 0;
  
  prevState = currentState;

  if (digitalRead(PIN_DIGITAL_INPUT_NEUTRAL) == HIGH) {
    //ニュートラルに入ったらLOW
    Serial.println( "LOW" );
    currentState = LOW;
  } else {
    //CDSの電圧
    int i = analogRead(PIN_ANALOG_INPUT_CDS_SENSOR);
    float cdsV = i * 5.0 / 1023.0;
  
    //ON/OFF閾値 
    float threshold = 2.5;
    
    if (cdsV > threshold) {
      currentState = HIGH;
    } else if (cdsV < threshold - 0.2) {
      currentState = LOW;
    }
  }
  
  if (currentState != prevState) {
    timerStart = true;
    timer = millis();
  }

  /* ヘッドライトの点灯チェック */
  //1秒暗い状態が続いたらヘッドライトを点灯
  if (timerStart && 
        currentState == HIGH &&
          (millis() - timer > 1000)) {
    digitalWrite(PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY, HIGH);
    timerStart = false;
  }

  /* ヘッドライトの消灯チェック */
  //2.5秒明るい状態が続いたらヘッドライトを消灯
  if (timerStart && 
        currentState == LOW &&
          (millis() - timer > 2500)) {
    digitalWrite(PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY, LOW);
    timerStart = false;
  }
}


/*
 * turnSignalControl ウインカーの制御
 *   左ウインカースイッチ
 *   右ウインカースイッチ
 *   ウインカーキャンセルスイッチ
 *   からの信号をチェックし、それぞれの信号に従い、ウインカーをOn/Offする
 */
void turnSignalControl() {
  
  // 左ウインカースイッチのチェック
  int tl = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT_SW);
  if (tl == HIGH) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, HIGH);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
    CurrentTurnSignalState = ETurnSignalState::ON;
  }

  // 右ウインカースイッチのチェック
  int tr = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW);
  if (tr == HIGH) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, HIGH);
    CurrentTurnSignalState = ETurnSignalState::ON;
  }
  
  // ウインカーキャンセルスイッチのチェック
  int tc = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW);
  if (tc == HIGH) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
    CurrentTurnSignalState = ETurnSignalState::OFF;
  }
}


/*
 * calcMovingSpeed 車速の計算
 */
void calcMovingSpeed() {
  const int interval = 250;   //計測間隔 (単位:ミリ秒)
  static int prevSpeed = 0;   //前回チェックした際の速度
  static long timer = 0;

  static float aveSpeed[] = {0, 0, 0, 0};
  static int aveLength = 4;
  static int aveIndex = 0;
  
  if (millis() - timer > interval) {
    //interval(ミリ秒)の間に何メートル進んだか
    float m = PulseCount / NUMBER_OF_PULSES_PER_METER;
    
    //時速に変換
    // 「interval(ミリ秒)の間に進んだ距離」を「1秒で進んだ距離」に換算し、km/hを計算
    aveSpeed[aveIndex] = (m * (1000 / interval) / 1000) * 3600;
    
    //過去4回の計測分の平均を計算し、現在の速度とする
    float sum = 0;
    for ( int i = 0; i <= aveLength - 1; i++ ) {
      sum = sum + aveSpeed[i];
    }
    
    CurrentSpeed = sum / aveLength;
    
    aveIndex++;
    if ( aveIndex == aveLength ) { aveIndex = 0; }
    
    
    //前回チェックした際の速度と現在の速度を比較し、走行状態を判定
    if (CurrentSpeed >= 40) {
      //通常走行中（40km/h以上）
      CurrentSpeedState = ESpeedState::NORMAL_RUNNING;
      
    } else if (CurrentSpeed <= 25) {
      //ほぼ停止（25km/h以下）
      CurrentSpeedState = ESpeedState::ALMOST_STOP;
            
    } else if ( prevSpeed > CurrentSpeed ) {
      //減速中
      CurrentSpeedState = ESpeedState::SLOW_DOWN;
      
    } else {
      //徐行中
      CurrentSpeedState = ESpeedState::SLOW_RUNNING;
      
    }
    
    prevSpeed = CurrentSpeed;
    PulseCount = 0;
    timer = millis();
  }
}


/*
 * pulseCounter 速度計測のための割込み処理
 */
void pulseCounter() {
  PulseCount += 1;
}


/*
 * turnSignalAutoCancelControl ウインカーオートキャンセルの制御
 */
void turnSignalAutoCancelControl() {
  static ESpeedState prevSpeedState = ESpeedState::ALMOST_STOP;         //車速変化の前回チェックした際の状態
  static ETurnSignalState prevTurnSignalState = ETurnSignalState::OFF;  //ウインカーの  〃
  static bool timerStart = false;
  static long timer = 0;
  static int lightingTime = 0;

  if (CurrentTurnSignalState == ETurnSignalState::ON) {
    
    switch (CurrentSpeedState) {
      case ESpeedState::NORMAL_RUNNING:
      
        //通常走行中にウインカースイッチをOFF→ON
        if (prevTurnSignalState == ETurnSignalState::OFF) {
                  
          //タイマースタート
          timer = millis();
          timerStart = true;
      
          if (CurrentSpeed > 90) {
            //90km/h以上は、3秒後にOFF
            lightingTime = 3000;
      
          } else {
            //90km/h未満は、2秒後にOFF
            lightingTime = 2000;
            
          }
        //「徐行中」から「通常走行中」状態へ移行したら、タイマースタート
        } else if (prevSpeedState == ESpeedState::SLOW_RUNNING) {
          
          //タイマースタート
          timer = millis();
          timerStart = true;
          lightingTime = 1000;
        }
        break;
    
      case ESpeedState::SLOW_RUNNING:
      
        //「ほぼ停止」または「減速中」から「徐行中」状態へ移行したら、タイマースタート
        if (prevSpeedState == ESpeedState::ALMOST_STOP ||
            prevSpeedState == ESpeedState::SLOW_DOWN) {
          
          //タイマースタート
          timer = millis();
          timerStart = true;
          
          if (prevSpeedState == ESpeedState::ALMOST_STOP) {
              //「ほぼ停止」から移行した場合、1秒後にOFF
              lightingTime = 1000;
              
          } else if (prevSpeedState == ESpeedState::SLOW_DOWN) {
              //「減速」から移行した場合、2秒後にOFF
              lightingTime = 2000;
              
          }
        }
        break;
        
      case ESpeedState::SLOW_DOWN:
        //「減速中」はウインカーOFFしない
        timerStart = false;
        break;
        
      case ESpeedState::ALMOST_STOP:
        //「ほぼ停止」はウインカーOFFしない
        timerStart = false;
        break;
        
    }
    
    //ウインカーOFF判定
    if ((millis() - timer > lightingTime) && timerStart) {
      //ウインカーOFF
      digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
      digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
      CurrentTurnSignalState = ETurnSignalState::OFF;
      
      timerStart = false;
    }
  }
  
  prevTurnSignalState = CurrentTurnSignalState;
  prevSpeedState = CurrentSpeedState;
}

