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

int toSend[5] = {0,0,0,0,0};
int toRead[5] = {0,0,0,0,0};

// ------------------- Pines -------------------

#define PIN_MAIN      2
#define PIN_RECEIVE   3
#define PIN_READ      A0
#define PIN_GATE      11   
#define PIN_RELE      A5   

#define LED_DOT       8
#define LED_LINE      9
#define LED_END       10

// ------------------- LCD -------------------

#define LCD_COLS 16
char lcd_history[LCD_COLS+1] =
"                ";

// ------------------- Flags ISR -------------------
// ------------------- Flags ISR -------------------
volatile bool startSend = false;
volatile bool startReceive = false;

// Añade estos timestamps para debounce
volatile unsigned long lastMainISR = 0;
volatile unsigned long lastReceiveISR = 0;
#define DEBOUNCE_MS 200


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
    pinMode(PIN_RELE, OUTPUT);

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

    }

    if(startReceive){

        readSignal();
        
        startReceive = false;
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
    unsigned long now = millis();
    if(now - lastMainISR > DEBOUNCE_MS){
        lastMainISR = now;
        startSend = true;
    }
}

void receiveISR(){
    unsigned long now = millis();
    if(now - lastReceiveISR > DEBOUNCE_MS){
        lastReceiveISR = now;
        startReceive = true;
    }
}


// ======================================================
// Lectura Morse local
// ======================================================

void readEntry(){
    Serial.println("S|WRITING"); // STATE|ESCRIBIENDO
    int read = 0;
    int i = 0;

    while(read != 2){

        while(analogRead(PIN_READ)<= 50);
        unsigned long start = millis();

        delay(20);

        while(analogRead(PIN_READ)> 50){
            unsigned long held = millis()-start;
            digitalWrite(LED_DOT,held>0);
            digitalWrite(LED_LINE,held>2000);
            digitalWrite(LED_END,held>3000);
        }

        delay(20);

        digitalWrite(LED_DOT,LOW);
        digitalWrite(LED_LINE,LOW);
        digitalWrite(LED_END,LOW);

        unsigned long duration = millis() - start;

        if(duration <= 2000)
            read = 0;
        else if(duration <= 3000)
            read = 1;
        else if(duration > 3000)
            read = 2;

        if(read == 2){          // Si es END, termina sin guardar
            toSend[i] = 2;      // Pone el terminador en la posición actual
            break;              // Sale limpiamente
        }

        toSend[i++] = read;     // Solo guarda si es punto o raya
        
        if(i >= 5){
            toSend[4] = 2;
            read = 2;
        }
    }

    char letra = decode_morse(toSend,5);
    Serial.print("T|"); // Tx|Letra
    Serial.println(letra);
    showInDisplay(letra,true);
    sendSignal();
    Serial.println("S|IDLE");

}



// ======================================================
// Enviar señal
// ======================================================

void sendSignal(){
    Serial.println("S|SENDING");
    digitalWrite(PIN_RELE,HIGH);
    delay(20);

    // Deshabilitar ISRs para que el pulso de salida
    // no se retroalimente ni el ruido dispare nada
    detachInterrupt(digitalPinToInterrupt(PIN_MAIN));
    detachInterrupt(digitalPinToInterrupt(PIN_RECEIVE));

    for(int j = 0; j < 5; j++){
        int sym = toSend[j];
        if(sym == 2){
            digitalWrite(PIN_GATE, HIGH);
            delay(600);
            digitalWrite(PIN_GATE, LOW);
            delay(100);
            break;
        }
        digitalWrite(PIN_GATE, HIGH);
        delay(sym == 0 ? 200 : 400);
        digitalWrite(PIN_GATE, LOW);
        delay(100);
    }

    // Limpiar flags que pudieran haberse acumulado
    startSend = false;
    startReceive = false;

    // Re-habilitar ISRs
    attachInterrupt(digitalPinToInterrupt(PIN_MAIN), beginEntryISR, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_RECEIVE), receiveISR, RISING);

    digitalWrite(PIN_RELE,LOW);
    Serial.println("S|IDLE");
}

// ======================================================
// Recibir señal
// ======================================================

void readSignal(){

    Serial.println("S|READING"); // STATE|LEYENDO_MORSE

    int read=0;
    int index=0;

    while(read!=2){

        while(digitalRead(PIN_RECEIVE)==LOW );
        unsigned long start = millis();

        while(digitalRead(PIN_RECEIVE)==HIGH);
        unsigned long duration = millis()-start;


        if(duration<400){
            read=0;
        }
        else if(duration<600){
            read=1;
        }
        else{
            read=2;
        }

        toRead[index++] = read;

        if(index>=5){
            toRead[4]=2;
           read = 2;
        }

    }

    char letra = decode_morse(toRead,5);
    Serial.print("R|");
    Serial.println(letra);  // Rx|Letra
    showInDisplay(letra,false);
    Serial.println("S|IDLE");
    
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