/*
  Arduino Pro Miniにて、
    バイクの
      ・ヘッドライトの自動点灯
      ・ウインカーのOn/Off
        →自動キャンセル(未実装)
    を行う

    created 2017/07/02 高橋夏彦
*/

//ヘッドライト関連
const int PIN_ANALOG_INPUT_CDS_SENSOR = 14;                 //CDSからの電圧
const int PIN_ANALOG_INPUT_HEADLIGHT_ONOFF_THRESHOLD = 15;  //5Vを半固定抵抗で分圧した、ヘッドライトOn/Offの閾値電圧
const int PIN_DIGITAL_OUTPUT_HEADLIGHT_SW = 9;              //ヘッドライトのリレー
int prevState = LOW;
int currentState = LOW;
bool timerStart = false;
long timer = 0;

//ウインカー関連
const int PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT = 7;            //ウインカー左スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT = 8;           //ウインカー右スイッチからの信号
const int PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL = 4;          //ウインカーキャンセルスイッチからの信号
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_SW = 6;        //ウインカー左のリレー
const int PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_SW = 5;       //ウインカー右のリレー

//速度計測
const int PIN_INTERRUPT_SPEED = 2;
volatile long wheelCount = 0;

/* setup() */
void setup() {
  pinMode(PIN_DIGITAL_OUTPUT_HEADLIGHT_SW, OUTPUT);
  
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT, INPUT);
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT, INPUT);
  pinMode(PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL, INPUT);  
  pinMode(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_SW, OUTPUT);
  pinMode(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_SW, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(PIN_INTERRUPT_SPEED), wheelCounter, RISING );
  
  Serial.begin( 9600 );
}

/* loop() */
void loop() {
  //delay(500);

  //ヘッドライトの制御
  headLightControl();

  //ウインカーの制御
  turnSignalControl();

  //Serial.println( wheelCount );
}

/*
  headLightControl ヘッドライトの制御
    CDSからの電圧をチェックし、一定時間、閾値を上回ったらヘッドライトを点灯する
                  〃                           下回ったらヘッドライトを消灯する
*/
void headLightControl() {
  int i = 0;
  
  //CDSの電圧
  i = analogRead(PIN_ANALOG_INPUT_CDS_SENSOR);
  float f = i * 5.0 / 1023.0;
  //Serial.print( "CDS:" );
  //Serial.println( f );

  //ON/OFF閾値 
  i = analogRead(PIN_ANALOG_INPUT_HEADLIGHT_ONOFF_THRESHOLD);
  float threshold = i * 5.0 / 1023.0;
  //Serial.print( "Thrhld:" );
  //Serial.println( threshold );

  prevState = currentState;
  if (f > threshold) {
    currentState = HIGH;
  } else if (f < threshold - 0.3) {
    currentState = LOW;
  }

  if (currentState != prevState) {
    timerStart = true;
    timer = millis();
  }

  /* ヘッドライトの点灯チェック */
  //0.5秒暗い状態が続いたらヘッドライトを点灯
  if (timerStart && 
        currentState = HIGH &&
          (millis() - timer > 1500)) {
    digitalWrite(PIN_DIGITAL_OUTPUT_HEADLIGHT_SW, HIGH);
    timerStart = false;
  }

  /* ヘッドライトの消灯チェック */
  //3秒明るい状態が続いたらヘッドライトを消灯
  if (timerStart && 
        currentState = LOW &&
          (millis() - timer > 3000)) {
    digitalWrite(PIN_DIGITAL_OUTPUT_HEADLIGHT_SW, LOW);
    timerStart = false;
  }
}

/*
  turnSignalControl ウインカーの制御
    左ウインカースイッチ
    右ウインカースイッチ
    ウインカーキャンセルスイッチ
    からの信号をチェックし、それぞれの信号に従い、ウインカーをOn/Offする
*/
void turnSignalControl() {
  // 左ウインカースイッチのチェック
  int tl = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_LEFT);
  if (tl == HIGH) {
    //Serial.println( "LEFT ON" );
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_SW, HIGH);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_SW, LOW);
  }

  // 右ウインカースイッチのチェック
  int tr = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_RIGHT);
  if (tr == HIGH) {
    //Serial.println( "RIGHT ON" );
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_SW, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_SW, HIGH);
  }
  
  // ウインカーキャンセルスイッチのチェック
  int tc = digitalRead(PIN_DIGITAL_INPUT_TURNSIGNAL_CANCEL);
  if (tc == HIGH) {
    //Serial.println( "CANCEL" );    
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_LEFT_SW, LOW);
    digitalWrite(PIN_DIGITAL_OUTPUT_TURNSIGNAL_RIGHT_SW, LOW);
  }
}

/*
  wheelCounter 速度計測のための割込み処理
    今後の作業
*/
void wheelCounter() {
  wheelCount += 1;
}


