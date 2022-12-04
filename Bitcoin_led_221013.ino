#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 12

//wifi 정보
const char* ssid = "iPhone";
const char* pw = "hyun0663";

//unsigned long time_prev, time_curr;
int cnt = 0, page = 0;  //설정 변동률 저장 변수, 페이지 번호 저장 번수 
String payload;
int start_idx, end_idx ;
String prc, ch;
int price, d_price;  // 현재 가격 저장 변수, 비교 가격 저장 변수
float ch_rate;  // 변동률 저장 변수
unsigned long timer = millis();

ESP8266WiFiMulti WiFiMulti;
HTTPClient https;

// 12비트 픽셀 12번 핀을 사용하는 led 객체 생성
Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, PIN, NEO_GRB + NEO_KHZ800);

// I2C 채널 할당 및 16문자 2행 출력하는 display 객체
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  pinMode(14,INPUT);
  pinMode(15,INPUT);
  pinMode(16,INPUT);
  pinMode(13, OUTPUT);

  // 진동모터 초기화
  digitalWrite(13, LOW);
  
  // wifi 연결 시작
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, pw);

  // lcd 초기화
  lcd.begin();
  lcd.backlight();
  
  // led 초기화
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  strip.begin();
  strip.show();
  setColor(strip.Color(255, 255, 0));
  
  // wifi 연결을 기다리는 상태
  lcd_wifi_wait();
  while (WiFiMulti.run() != WL_CONNECTED)
  {
    delay(300);
    Serial.print(".");
  }

  // wifi 연결 알림 및 시리얼 모니터에 IP 정보 출력
  lcd_wifi_conn();
  delay(300);
  
  /*Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP()); 테스트 코딩*/

  // 변동률 확인을 위한 초기 비교 가격 설정
  get_btcinfo();
  lcd_page1();
  // 가격 변동 시 LED 색상 변경
  if(ch == "RISE"){
    setColor(strip.Color(255, 0, 0)); // 가격 상승시 빨간색
  }
  else if(ch == "FALL"){
    setColor(strip.Color(0, 0, 255)); // 가격 하락시 파란색
  }
  else if(ch == "EVEN") {
    setColor(strip.Color(255, 255, 0)); //가격 유지시 노란색
  }
}

void loop() {
  // 10초동안 스위치 이벤트 검사 후 10초에 한 번씩 비트코인 시세 업로드
  if (timer + 10000 > millis()){
    boolean page_upload = page_cnt();
    fluc_set();
    // 변동률이 설정되었을 때(변동율 설정 페이지에서 가격정보 페이지로 이동시)
    if (page_upload == true) {
      lcd_page1(); // 가격정보 페이지 디스플레이
      d_price = price; // 현재 가격을 비교가로 지정
    }
    else if (page == 1) {
      lcd_page2(); // 변동률 설정페이지
    }
  }
  else {
    get_btcinfo(); // 업비트에서 현재 가격정보 불러옴
    if (page == 0) {
      lcd_page1();
      if(cnt != 0){ // 사용자가 알림을 설정하면
        diff_rate();  // 설정 변동률 이상 변화 발생 시 알림
      }
    }
    // 가격 변동 시 LED 색상 변경
    if(ch == "RISE"){
      setColor(strip.Color(255, 0, 0)); // 가격 상승시 빨간색
    }
    else if(ch == "FALL"){
      setColor(strip.Color(0, 0, 255)); // 가격 하락시 파란색
    }
    else if(ch == "EVEN") {
      setColor(strip.Color(255, 255, 0)); //가격 유지시 노란색
    }
    timer = millis();
  }
}


// -------------------------------------------------------------------------------------------------------------
                                                   /* led */
// LED RGB 색상으로 출력
void setColor(uint32_t c) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
  }
  strip.show();
}

/*
// LED 특정 색상으로 깜박임
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();
     
      delay(wait);
     
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}*/

// ---------------------------------------------------------------------------------------------------------------
                                                  /* lcd display */
// wifi 연결하는 동안 lcd 출력
void lcd_wifi_wait(){
  lcd.setCursor(0, 0);
  lcd.print("Connect To WiFi");
  lcd.setCursor(2, 1);
  lcd.print("Please Wait");
}

// wifi 연결완료 lcd 출력
void lcd_wifi_conn(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IP");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
}

// 비트코인 가격정보 페이지
void lcd_page1(){
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BTC");
  lcd.setCursor(4, 0);
  if (ch == "FALL"){
    lcd.print("-");
  }
  else {
    lcd.print("+");
  }
  lcd.setCursor(5, 0);
  lcd.print(ch_rate);
  lcd.setCursor(9, 0);
  lcd.print("%");
  lcd.setCursor(0, 1);
  lcd.print("KRW");
  lcd.setCursor(4, 1);
  lcd.print(price);
}

// 등락률 설정 페이지
void lcd_page2(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Fuct Rate Set");
  lcd.setCursor(0,1);
  lcd.print(cnt);
}

// ----------------------------------------------------------------------------------------------------------------
                                                    /* swich */
// 페이지 이동 스위치 함수 
boolean page_cnt(){
  int flag1 = digitalRead(16);
  delay(200);
  
  if(flag1 == 1){
    if(page == 0) {
      page = 1;
      return false;
    } else {
      page = 0;
      return true;
    }
  }
  return false;
}

// 변동율 설정 함수
void fluc_set(){
  int flag3 = digitalRead(15);
  int flag4 = digitalRead(14);
  delay(200); 
  if(flag3 == 1 && cnt != 100){
    cnt++;
  }
  else if(flag4 == 1 && cnt != 0){
    cnt--;
  }
}

//-------------------------------------------------------------------------------------------------------------------
                                                   /* Upbit API */

// 비트코인 정보 가져오는 함수
void get_btcinfo(){
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    https.begin(*client, "https://api.upbit.com/v1/ticker?markets=KRW-BTC");
    int httpCode = https.GET();

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      payload = https.getString();

      // price 변수에 BTC 시가 저장
      start_idx = payload.indexOf("trade_price") + 13;
      end_idx = payload.indexOf("prev_closing_price") - 2;
      prc = payload.substring(start_idx, end_idx);
      price = (float)prc.toInt();

      // ch_rate 변수에 변동률 저장
      start_idx = payload.indexOf("change_rate") + 13;
      end_idx = payload.indexOf("signed_chage_price") - 2;
      prc = payload.substring(start_idx, end_idx);
      ch_rate = prc.toFloat() * 100;

      // ch 변수에 가격 변동 상태 정보 저장(EVEN | RISE | FALL) 
      start_idx = payload.indexOf("change") + 9;
      end_idx = payload.indexOf("change_price") - 3;
      ch = payload.substring(start_idx, end_idx);
    }
  }
}


//-------------------------------------------------------------------------------------------------------------------
                                                   /* 가격 변동 알림 설정 */

void diff_rate(){
  int diff = price - d_price;
  float diff_rate;
  // diff_rate(%)이상 상승시 알림
  if(diff >= 0) {
    diff_rate = ((float)diff / (float)d_price) * 100;
    if(diff_rate >= 0.02) {
     d_price = price;
     // 5초간 알림
     for(int i = 0; i < 5; i++){
      digitalWrite(13, HIGH);
      setColor(strip.Color(255, 0, 255)); // LED 색상: 분홍색
      delay(1000); 
      digitalWrite(13, LOW);
     }
     /*Serial.print("Change: ");
     Serial.println(d_price); //테스트 코드*/
    }
  }
  // diff_rate(%)이상 하락시 알림
  else if(diff < 0) {
    diff *= -1;
    diff_rate = ((float)diff / (float)d_price) * 100;
    Serial.println(diff_rate);
    if(diff_rate >= 0.02) {
     d_price = price;
     // 5초간 알림
     for(int i = 0; i < 5; i++){
        digitalWrite(13, HIGH);
       setColor(strip.Color(0, 255, 0)); // LED 색상: 초록
        delay(1000); 
        digitalWrite(13, LOW);
      }
      /*Serial.print("Change: ");
      Serial.println(d_price); //테스트 코드*/
    }
  }
  Serial.print("d_price: ");
  Serial.println(d_price);
  Serial.print("price: ");
  Serial.println(price);
  Serial.print("diff: ");
  Serial.println(diff);
  Serial.print("diff_rate: ");
  Serial.println(diff_rate);
  Serial.println(); //테스트 코드
}
