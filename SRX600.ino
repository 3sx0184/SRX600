/*
 * Arduino Pro Miniにて、
 *  バイクの
 *    ・ヘッドライトの自動点灯
 *    ・ウインカーのOn/Off
 *      →自動キャンセル(未実装)
 *  を行う
 *
 *  created 2017/07/02 高橋夏彦
 *  updated 2017/07/15 速度計測対応
 */

//ヘッドライト関連
const int PIN_ANALOG_INPUT_CDS_SENSOR = 14;                   //CDSからの電圧
const int PIN_ANALOG_INPUT_HEADLIGHT_ONOFF_THRESHOLD = 15;    //5Vを半固定抵抗で分圧した、ヘッドライトOn/Offの閾値電圧
const int PIN_DIGITAL_OUTPUT_HEADLIGHT_RELAY = 9;             //ヘッドライトのリレー
int prevState = LOW;
int currentState = LOW;
bool timerStart = false;
long timer = 0;

//ウインカー関連
const int PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT_SW = 7;           //ウインカー左スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW = 8;          //ウインカー右スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW = 4;         //ウインカーキャンセルスイッチからの信号
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY = 6;       //ウインカー左のリレー
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY = 5;      //ウインカー右のリレー
enum TurnSignalState {OFF = 0, ON = 1};                       //ウインカーの状態
TurnSignalState tunsignalState = TURNSIGNAL_STATE.OFF;

//速度計測
const int PIN_INTERRUPT_SPEED_PULSE = 2;                      //回転速度センサーからの割込みピン
const float NUMBER_OF_PULSES_PER_METER = 23.0;                //1mあたりのパルス数(ドライブスプロケットからの検出数)
volatile long pulseCount = 0;                                 //回転速度センサーからのパルス数
long speedPulseTimer = 0;
int currentSpeed = 0;

//ウインカーオートキャンセル
enum SpeedState {UP = 0, DOWN = 1, KEEP = 2, STOP = 3};       //車速変化の状態(加速、減速、等速、停止)


/* setup() */
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

/* loop() */
void loop() {
  //delay(1000);

  //ヘッドライトの制御
  headLightControl();

  //ウインカーの制御
  turnSignalControl();

  //車速の計算
  calcMovingSpeed();

}

/*
 * headLightControl ヘッドライトの制御
 *   CDSからの電圧をチェックし、一定時間、閾値を上回ったらヘッドライトを点灯する
 *                 〃                           下回ったらヘッドライトを消灯する
 */
void headLightControl() {
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
    turnsignaleState = TurnsignaleState::ON;
  }

  // 右ウインカースイッチのチェック
  int tr = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT_SW);
  if (tr == HIGH) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, HIGH);
    turnsignaleState = TurnsignaleState::ON;
  }
  
  // ウインカーキャンセルスイッチのチェック
  int tc = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL_SW);
  if (tc == HIGH) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
    turnsignaleState = TurnsignaleState::OFF;
  }
}

/*
 * calcMovingSpeed 車速の計算
 */
void calcMovingSpeed() {
  //計測間隔 (単位:ミリ秒)
  const int interval = 500;
  
  if (millis() - speedPulseTimer > interval) {
    //interval(ミリ秒)の間に何メートル進んだか
    float m = pulseCount / NUMBER_OF_PULSES_PER_METER;
    
    //時速に変換
    // 「interval(ミリ秒)の間に進んだ距離」を「1秒で進んだ距離」に換算し、km/hを計算
    currentSpeed = (m * (1000 / interval) / 1000) * 3600;
    
    speedPulseTimer = millis();
    pulseCount = 0;
    
    Serial.println( currentSpeed );
  }
}

/*
 * pulseCounter 速度計測のための割込み処理
*/
void pulseCounter() {
  pulseCount += 1;
}

/*
 * turnSignalAutoCancelControl ウインカーオートキャンセルの制御
 *   加速/等速状態が一定の時間維持されたら、ウインカーをOffする
 */
void turnSignalAutoCancelControl() {
  static SpeedState currentSpeedState = SpeedState::STOP;  //車速変化の現在の状態
  static SpeedState prevSpeedState = SpeedState::STOP;     //車速変化の前回チェックした際の状態
  static long speedChangeTimer = 0;                        //速度変化の継続時間測定用タイマー開始時間
  static int prevSpeed = 0;                                //前回チェックした際の速度
  
  //ウインカーがOFFなら何もしない
  if (turnsignaleState == TurnsignaleState.OFF) return;
  
  //前回チェックした際の速度と現在の速度を比較し、走行状態を判定
  if (currentSpeed == 0) {
    currentSpeedState = SpeedState::STOP;
  } else if (prevSpeed == currentSpeed) {
    currentSpeedState = SpeedState::KEEP;
  } else if (prevSpeed < currentSpeed) {
    currentSpeedState = SpeedState::UP;
  } else {
    currentSpeedState = SpeedState::DOWN;
  }
  
  //前回チェックした際と走行状態が異なっていたら、タイマースタート
  if (prevSpeedState != currentSpeedState) {
    speedChangeTimer = millis();
  }
  
  //3秒間、等速/加速状態が続いたら、ウインカーOFF
  if ((millis() - speedChangeTimer > 3000) &&
        (currentSpeedState == SpeedState::UP ||
           currentSpeedState == SpeedState::KEEP)) {
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_RELAY, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_RELAY, LOW);
    turnsignaleState = TurnsignaleState::OFF;
  }
  
  prevSpeedState = currentSpeedState;
  prevSpeed = currentSpeed;
}

