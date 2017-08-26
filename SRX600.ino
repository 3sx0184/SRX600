/*
 * Arduino Pro Miniにて、
 *  バイクの
 *    ・ヘッドライトの自動点灯
 *    ・ウインカーのOn/Off 自動キャンセル
 *  を行う
 *
 *  created 2017/07/02 高橋夏彦
 *  updated 2017/07/15 速度計測対応
 */


//ヘッドライト関連
const int PIN_ANALOG_INPUT_CDS_SENSOR = 14;                   //CDSからの電圧
const int PIN_ANALOG_INPUT_HEADLIGHT_ONOFF_THRESHOLD = 15;    //5Vを半固定抵抗で分圧した、ヘッドライトOn/Offの閾値電圧
const int PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY = 9;             //ヘッドライトのリレー

//ウインカー関連
const int PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT_SW = 7;           //ウインカー左スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW = 8;          //ウインカー右スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW = 4;         //ウインカーキャンセルスイッチからの信号
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY = 6;       //ウインカー左のリレー
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY = 5;      //ウインカー右のリレー
enum ETurnSignalState {OFF = 0, ON = 1};                      //ウインカーの状態
ETurnSignalState CurrentTurnSignalState
                                = ETurnSignalState::OFF;

//速度計測関連
const int PIN_INTERRUPT_SPEED_PULSE = 2;                      //回転速度センサーからの割込みピン
const float NUMBER_OF_PULSES_PER_METER = 23.0;                //1mあたりのパルス数(ドライブスプロケットからの検出数)
volatile int PulseCount = 0;                                  //回転速度センサーからのパルス数 
int CurrentSpeed = 0;                                         //現在の車速

//車速変化の状態(加速、減速、等速、停止または徐行中)
enum ESpeedState {UP = 0,
                  DOWN = 1,
                  KEEP = 2,
                  STOP_OR_SLOW = 3};

ESpeedState CurrentSpeedState = ESpeedState::STOP_OR_SLOW;            //車速変化の現在の状態


/*
 * setup() 
 * 
 */
void setup() {
  pinMode(PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY, OUTPUT);
  
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
//  Serial.println( CurrentSpeed );
//  
//  if ( CurrentSpeedState == ESpeedState::KEEP ) {
//    Serial.println( "KEEP" );
//  } else if ( CurrentSpeedState == ESpeedState::STOP_OR_SLOW ) {
//    Serial.println( "STOP_OR_SLOW" );
//  } else if ( CurrentSpeedState == ESpeedState::UP ) {
//    Serial.println( "UP" );
//  } else {
//    Serial.println( "DOWN" );
//  }

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

  int i = 0;
  
  //CDSの電圧
  i = analogRead(PIN_ANALOG_INPUT_CDS_SENSOR);
  float cdsV = i * 5.0 / 1023.0;

  //ON/OFF閾値 
  i = analogRead(PIN_ANALOG_INPUT_HEADLIGHT_ONOFF_THRESHOLD);
  float threshold = i * 5.0 / 1023.0;

  prevState = currentState;
  if (cdsV > threshold) {
    currentState = HIGH;
  } else if (cdsV < threshold - 0.2) {
    currentState = LOW;
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
    if (CurrentSpeed <= 25) {
      //停止、または徐行中（25km/h以下）
      CurrentSpeedState = ESpeedState::STOP_OR_SLOW;
      
    } else if ( CurrentSpeed == prevSpeed ) {
      //等速運転中
      CurrentSpeedState = ESpeedState::KEEP;
      
    } else if ( prevSpeed < CurrentSpeed ) {
      //加速中
      CurrentSpeedState = ESpeedState::UP;
      
    } else {
      //減速中
      CurrentSpeedState = ESpeedState::DOWN;
      
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
  static ESpeedState prevSpeedState = ESpeedState::STOP_OR_SLOW;        //車速変化の前回チェックした際の状態
  static ETurnSignalState prevTurnSignalState = ETurnSignalState::OFF;  //ウインカーの　〃
  static bool timerStart = false;
  static long timer = 0;
  static int lightingTime = 0;

  //走行中にウインカースイッチをOFF→ON
  if (CurrentTurnSignalState == ETurnSignalState::ON &&
        prevTurnSignalState != CurrentTurnSignalState &&
          CurrentSpeed > 40) {
    //タイマースタート
    timer = millis();
    timerStart = true;

    //2秒後にOFF
    lightingTime = 2000;
    
  }
  
  if ((prevSpeedState == ESpeedState::STOP_OR_SLOW ||
        prevSpeedState == ESpeedState::DOWN) &&
          (CurrentSpeedState == ESpeedState::UP ||
           CurrentSpeedState == ESpeedState::KEEP)) {
    //「減速/停止/徐行中」から「加速/等速」状態へ移行したら、タイマースタート
    
    //タイマースタート
    timer = millis();
    timerStart = true;
    
    if (prevSpeedState == ESpeedState::STOP_OR_SLOW) {
        //停止/徐行中から移行した場合、1秒後にOFF
        lightingTime = 1000;
        
    } else if (prevSpeedState == ESpeedState::DOWN) {
        //減速から移行した場合、2秒後にOFF
        lightingTime = 2000;
        
    }
    
  } else if (CurrentSpeedState == ESpeedState::STOP_OR_SLOW || 
              (CurrentSpeedState == ESpeedState::DOWN && CurrentSpeed < 40)) {
                
    //「減速/停止/徐行中」はウインカーOFFしない
    timerStart = false;
    
  }
  
  //ウインカーOFF判定
  if ((millis() - timer > lightingTime) && timerStart) {
    //ウインカーOFF
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
    CurrentTurnSignalState = ETurnSignalState::OFF;
    
    timerStart = false;
  }
  
  prevTurnSignalState = CurrentTurnSignalState;
  prevSpeedState = CurrentSpeedState;
}

