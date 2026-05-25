#include <LiquidCrystal.h>

LiquidCrystal lcd(A1, A2, 4, 5, 6, 7);

// ------------------- Morse -------------------

static const char MORSE_TREE[] = {
    0,'E','T','I','A','N','M',
    'S','U','R','W','D','K','G','O',
    'H','V','F',0,'L',0,'P','J',
    'B','X','C','Y','Z','Q',0,'_'
};

const int MORSE_ENCODE[27][5] = {
 //A                B
 {0,1,2,0,0}, {1,0,0,0,2},
 //C                D
 {1,0,1,0,2}, {1,0,0,2,0},
 //E                F
 {0,2,0,0,0}, {0,0,1,0,2},
 //G                H
 {1,1,0,2,0}, {0,0,0,0,2},
 //I                J
 {0,0,2,0,0}, {0,1,1,1,2},
 //K                L
 {1,0,1,2,0}, {0,1,0,0,2},
 //M                N
 {1,1,2,0,0}, {1,0,2,0,0},
 //O                P
 {1,1,1,2,0}, {0,1,1,0,2},
 //Q                R
 {1,1,0,1,2}, {0,1,0,2,0},
 //S                T
 {0,0,0,2,0}, {1,2,0,0,0},
 //U                V
 {0,0,1,2,0}, {0,0,0,1,2},
 //W                X
 {0,1,1,2,0}, {1,0,0,1,2},
 //Y                Z
 {1,0,1,1,2}, {1,1,0,0,2},
 // _
 {1,1,1,1,2}
};

#define TREE_SIZE (sizeof(MORSE_TREE))

int toSend[5];
int toRead[5];

// ------------------- Pines -------------------

#define PIN_MAIN      2
#define PIN_RECEIVE   3
#define PIN_READ      A0
#define PIN_GATE      11      

#define LED_DOT       8
#define LED_LINE      9
#define LED_END       10

// ------------------- LCD -------------------

#define LCD_COLS 16
char lcd_history[LCD_COLS+1] =
"                ";

// ------------------- Flags ISR -------------------

volatile bool startSend = false;
volatile bool startReceive = false;


// ======================================================
// SETUP
// ======================================================

void setup() {

    Serial.begin(9600);
    lcd.begin(16,2);

    pinMode(PIN_MAIN, INPUT);
    pinMode(PIN_RECEIVE, INPUT);

    pinMode(PIN_READ, INPUT);

    pinMode(PIN_GATE, OUTPUT);

    pinMode(LED_DOT, OUTPUT);
    pinMode(LED_LINE, OUTPUT);
    pinMode(LED_END, OUTPUT);

    digitalWrite(PIN_GATE,LOW);

    attachInterrupt(
        digitalPinToInterrupt(PIN_MAIN),
        beginEntryISR,
        RISING
    );

    attachInterrupt(
        digitalPinToInterrupt(PIN_RECEIVE),
        receiveISR,
        RISING
    );
    Serial.println("S|IDLE");
}

void loop() {
   
    if(startSend){

        startSend = false;

        readEntry();
        sendSignal();

    }

    if(startReceive){

        startReceive = false;

        readSignal();

    }

    if(Serial.available() > 0)
    {
      String signal = Serial.readStringUntil('\n');
      char id = signal[0];  // W -> Enviar lo que sigue; cualquier otra cosa sera un error
      if(id == 'W')
      {
        int i = 2;
        while(signal[i] != '0')
        {
          setCharAsMorse(signal[i]);
          Serial.print("T|");
          Serial.println(signal[i]);
          sendSignal();
          i++;
        }
        

      }

    }

}


// ======================================================
// ISR
// ======================================================

void beginEntryISR(){

    startSend = true;

}

void receiveISR(){

    startReceive = true;

}


// ======================================================
// Lectura Morse local
// ======================================================

void readEntry(){
    Serial.println("S|WRITING"); // STATE|ESCRIBIENDO
    int read = 0;
    int i = 0;

    while(read != 2){

        while(digitalRead(PIN_READ)==LOW);
        unsigned long start = millis();

        while(digitalRead(PIN_READ)==HIGH){
            unsigned long held = millis()-start;
            digitalWrite(LED_DOT,held>0);
            digitalWrite(LED_LINE,held>2000);
            digitalWrite(LED_END,held>3000);
        }

        digitalWrite(LED_DOT,LOW);
        digitalWrite(LED_LINE,LOW);
        digitalWrite(LED_END,LOW);

        unsigned long duration =  millis()-start;

        if(duration<=2000)
            read=0;

        else if(duration<=3000)
            read=1;

        else
            read=2;


        toSend[i++] = read;

        if(i>=5){
            toSend[4]=2;
            read=2;
        }
    }

    char letra = decode_morse(toSend,5);
    Serial.print("T|"); // Tx|Letra
    Serial.println(letra);
    showInDisplay(letra,true);
    Serial.println("S|IDLE");

}



// ======================================================
// Enviar señal
// ======================================================

void sendSignal(){

    Serial.println("S|SENDING"); // STATE|ENVIANDO

    for(int j=0;j<5;j++){

        int sym = toSend[j];

        digitalWrite(PIN_GATE,HIGH);

        if(sym==0){
            delay(200);
        }
        else if(sym==1){
            delay(400);
        }
        else{
            delay(600);
        }

        digitalWrite(PIN_GATE,LOW);
        delay(100);
        if(sym == 2)
        {
          break;
        }
    }
    Serial.println("S|IDLE");
}

// ======================================================
// Recibir señal
// ======================================================

void readSignal(){

    Serial.println("S|READING"); // STATE|LEYENDO_MORSE

    int read=0;
    int i=0;

    while(read!=2){

        while(digitalRead(PIN_RECEIVE)==LOW );
        unsigned long start = millis();

        while(digitalRead(PIN_RECEIVE)==HIGH);
        unsigned long duration = millis()-start;


        if(duration<400)
            read=0;
        else if(duration<600)
            read=1;
        else
            read=2;

        toRead[i++] = read;

        if(i>=5){
            toRead[4]=2;
            read=2;
        }

    }

    char letra = decode_morse(toRead,5);
    Serial.print("R|");
    Serial.println(letra);  // Rx|Letra
    showInDisplay(letra,false);
}



// ======================================================
// Decodificador Morse
// ======================================================

char decode_morse(const int code[],int len){

    int idx=0;
    for(int i=0;i<len;i++){

        if(code[i]==2)
            break;

        idx =idx*2 + 1 +code[i];

        if(idx>=TREE_SIZE)
            return '?';

    }

    char c = MORSE_TREE[idx];

    return c ? c : '?';

}



// ======================================================
// LCD
// ======================================================

void showInDisplay(char letra, bool enviada){

    for(int i=0;i<LCD_COLS-1;i++){

        lcd_history[i]=lcd_history[i+1];
    }

    lcd_history[LCD_COLS-1] = letra;
    lcd.setCursor(0,0);
    lcd.print(lcd_history);
    lcd.setCursor(0,1);
    lcd.print(enviada ?"TX: " : "RX: ");

    lcd.write(letra);

    lcd.print("            ");

}

void setCharAsMorse(char c){

    c = toupper(c);

    int idx;

    if(c == '_')
        idx = 26;

    else if(c >= 'A' && c <= 'Z')
        idx = c - 'A';

    else
        return;

    for( int i=0; i<5;i++ ){
        toSend[i] = MORSE_ENCODE[idx][i];
    }

}