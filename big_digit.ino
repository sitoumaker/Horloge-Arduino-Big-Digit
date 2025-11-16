#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define DS1307_ADDRESS 0x68
// L'adresse I2C est 0x27 (commune) ou 0x3F (moins commune).
// Si 0x27 ne fonctionne pas, essayez 0x3F. Je mets 0x27 par défaut.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =========================================================================
// === CARACTÈRES PERSONNALISÉS (Segments pour les chiffres 3x2) ===
// =========================================================================
byte LT[8] = // Segment 0: Top Gauche
{
  B00111,B01111,B11111,B11111,B11111,B11111,B11111,B11111
};
byte UB[8] = // Segment 1: Haut Milieu
{
  B11111,B11111,B11111,B00000,B00000,B00000,B00000,B00000
};
byte RT[8] = // Segment 2: Top Droit
{
  B11100,B11110,B11111,B11111,B11111,B11111,B11111,B11111
};
byte LL[8] = // Segment 3: Bas Gauche
{
  B11111,B11111,B11111,B11111,B11111,B11111,B01111,B00111
};
byte LB[8] = // Segment 4: Bas Milieu
{
  B00000,B00000,B00000,B00000,B00000,B11111,B11111,B11111
};
byte LR[8] = // Segment 5: Bas Droit
{
  B11111,B11111,B11111,B11111,B11111,B11111,B11110,B11100
};
byte MB[8] = // Segment 6: Milieu Horizontal (Barre centrale)
{
  B11111,B11111,B11111,B00000,B00000,B00000,B11111,B11111
};
byte block[8] = // Segment 7: Bloc plein
{
  B11111,B11111,B11111,B11111,B11111,B11111,B11111,B11111
};

// Index des caractères : 0=LT, 1=UB, 2=RT, 3=LL, 4=LB, 5=LR, 6=MB, 7=block

// ----- Prototypes -----
void reglerHeureRTC(int h, int m, int s = 0);
void lireHeureRTC(int &h, int &m, int &s);
void afficherHeure();
void showDigitAt(int column, int value, int lastValue);
void showDigit();
void digit0(); void digit1(); void digit2(); void digit3(); void digit4();
void digit5(); void digit6(); void digit7(); void digit8(); void digit9();
void gererChrono(bool haut, bool reset);
void reglerHeureMode(bool haut, bool reset, bool modeBtn);
byte bcdToDec(byte val);
byte decToBcd(byte val);
void print2Digits(int val);
void beep();

// === Broches ===
const int BTN_MODE = 10;
const int BTN_HAUT = 9;
const int BTN_RESET = 8;
const int BUZ  = 7;

// === États internes ===
// Les derniers chiffres pour le rafraîchissement
int lastMin1 = -1, lastMin2 = -1, lastHour1 = -1, lastHour2 = -1;
bool lastModeState = HIGH;
bool lastHautState = HIGH;
bool lastResetState = HIGH;

// === Modes ===
enum Mode { HORLOGE, CHRONO, REGLAGE };
Mode mode = HORLOGE;

// === Chrono ===
bool chronoEnMarche = false;
unsigned long chronoDernierMillis = 0;
int ch_ms = 0, ch_s = 0, ch_m = 0, ch_h = 0;

// === Réglage ===
int reglageHeure = 0, reglageMinute = 0;
bool reglerHeure = true;

// Variables globales pour le dessin des gros chiffres
int col = 0, number;

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();

  // Création des caractères personnalisés
  lcd.createChar(0, LT);
  lcd.createChar(1, UB);
  lcd.createChar(2, RT);
  lcd.createChar(3, LL);
  lcd.createChar(4, LB);
  lcd.createChar(5, LR);
  lcd.createChar(6, MB);
  lcd.createChar(7, block);

  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_HAUT, INPUT_PULLUP);
  pinMode(BTN_RESET, INPUT_PULLUP);
  pinMode(BUZ, OUTPUT);

  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Horloge + Chrono");
  lcd.setCursor(5,1); lcd.print("SITOU");
  delay(1500);
  lcd.clear();
}

void loop() {
  bool modeBtn = digitalRead(BTN_MODE);
  bool hautBtn = digitalRead(BTN_HAUT);
  bool resetBtn = digitalRead(BTN_RESET);

  // Gestion du changement de mode
  if (modeBtn == LOW && lastModeState == HIGH) {
    mode = (Mode)((mode + 1) % 3);
    beep();
    lcd.clear();
    delay(250);
  }
  lastModeState = modeBtn;

  switch (mode) {
    case HORLOGE: afficherHeure(); break;
    case CHRONO: gererChrono(hautBtn, resetBtn); break;
    case REGLAGE: reglerHeureMode(hautBtn, resetBtn, modeBtn); break;
  }

  lastHautState = hautBtn;
  lastResetState = resetBtn;
}

// === Affichage de l'heure avec gros chiffres ===
void afficherHeure() {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 3);

  int s = bcdToDec(Wire.read());
  int m = bcdToDec(Wire.read());
  int h = bcdToDec(Wire.read() & 0b00111111);

  // H1 (0), H2 (4), M1 (8), M2 (12)
  
  showDigitAt(0, h / 10, lastHour1); lastHour1 = h / 10;
  showDigitAt(4, h % 10, lastHour2); lastHour2 = h % 10;
  
  // Affichage du séparateur : (Colonne 7)
  if (s % 2) { // Fait clignoter
    lcd.setCursor(7, 0); lcd.print(":");
    lcd.setCursor(7, 1); lcd.print(":");
  } else {
    lcd.setCursor(7, 0); lcd.print(" ");
    lcd.setCursor(7, 1); lcd.print(" ");
  }

  showDigitAt(8, m / 10, lastMin1); lastMin1 = m / 10;
  showDigitAt(12, m % 10, lastMin2); lastMin2 = m % 10;
}

// === Gestion du dessin des gros chiffres ===
void showDigitAt(int column, int value, int lastValue) {
  // Le rafraîchissement est essentiel pour l'animation/clignotement
  if (value != lastValue) {
    // Efface l'ancienne position (Largeur de 3 caractères par chiffre)
    lcd.setCursor(column, 0); lcd.print("   ");
    lcd.setCursor(column, 1); lcd.print("   ");
  }
  col = column;
  number = value;
  showDigit();
}

void showDigit() {
  switch (number) {
    case 0: digit0(); break;
    case 1: digit1(); break;
    case 2: digit2(); break;
    case 3: digit3(); break;
    case 4: digit4(); break;
    case 5: digit5(); break;
    case 6: digit6(); break;
    case 7: digit7(); break;
    case 8: digit8(); break;
    case 9: digit9(); break;
  }
}

// =========================================================================
// === FONCTIONS DE DESSIN DES CHIFFRES (Version nettoyée) ===
// =========================================================================

void digit0() {
  lcd.setCursor(col,0);
  lcd.write(0);  // LT
  lcd.write(1);  // UB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.write(3);  // LL
  lcd.write(4);  // LB
  lcd.write(5);  // LR
}

void digit1() {
  lcd.setCursor(col,0);
  lcd.write(1);  // UB
  lcd.write(2);  // RT
  lcd.print(" ");
  lcd.setCursor(col,1);
  lcd.write(4);  // LB
  lcd.write(7);  // block
  lcd.write(4);  // LB
}

void digit2() {
  lcd.setCursor(col,0);
  lcd.write(6);  // MB
  lcd.write(6);  // MB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.write(3);  // LL
  lcd.write(4);  // LB
  lcd.write(4);  // LB
}

void digit3() {
  lcd.setCursor(col,0);
  lcd.write(6);  // MB
  lcd.write(6);  // MB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.write(4);  // LB
  lcd.write(4);  // LB
  lcd.write(5);  // LR
}

void digit4() {
  lcd.setCursor(col,0);
  lcd.write(3);  // LL
  lcd.write(4);  // LB
  lcd.write(7);  // block
  lcd.setCursor(col, 1);
  lcd.print(" ");
  lcd.print(" ");
  lcd.write(7);  // block
}

void digit5() {
  lcd.setCursor(col,0);
  lcd.write(3);  // LL
  lcd.write(6);  // MB
  lcd.write(6);  // MB
  lcd.setCursor(col, 1);
  lcd.write(4);  // LB
  lcd.write(4);  // LB
  lcd.write(5);  // LR
}

void digit6() {
  lcd.setCursor(col,0);
  lcd.write(0);  // LT
  lcd.write(6);  // MB
  lcd.write(6);  // MB
  lcd.setCursor(col, 1);
  lcd.write(3);  // LL
  lcd.write(4);  // LB
  lcd.write(5);  // LR
}

void digit7() {
  lcd.setCursor(col,0);
  lcd.write(1);  // UB
  lcd.write(1);  // UB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.print(" ");
  lcd.print(" ");
  lcd.write(7);  // block
}

void digit8() {
  lcd.setCursor(col,0);
  lcd.write(0);  // LT
  lcd.write(6);  // MB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.write(3);  // LL
  lcd.write(4);  // LB
  lcd.write(5);  // LR
}

void digit9() {
  lcd.setCursor(col,0);
  lcd.write(0);  // LT
  lcd.write(6);  // MB
  lcd.write(2);  // RT
  lcd.setCursor(col, 1);
  lcd.print(" ");
  lcd.print(" ");
  lcd.write(7);  // block
}


// === Chronomètre (utilise les petits chiffres standard) ===
void gererChrono(bool haut, bool reset) {
  if (haut == LOW && lastHautState == HIGH) {
    chronoEnMarche = !chronoEnMarche;
    beep();
    delay(150);
  }
  if (reset == LOW && lastResetState == HIGH && !chronoEnMarche) {
    ch_ms = ch_s = ch_m = ch_h = 0;
    beep();
    delay(150);
  }
  if (chronoEnMarche) {
    unsigned long maintenant = millis();
    int ecoule = maintenant - chronoDernierMillis;
    chronoDernierMillis = maintenant;
    ch_ms += ecoule;
    if (ch_ms >= 1000) { ch_ms -= 1000; ch_s++; }
    if (ch_s >= 60) { ch_s = 0; ch_m++; }
    if (ch_m >= 60) { ch_m = 0; ch_h++; }
  } else chronoDernierMillis = millis();

  lcd.setCursor(0,0); lcd.print("CHRONO: ");
  lcd.setCursor(0,1);
  print2Digits(ch_h); lcd.print(":");
  print2Digits(ch_m); lcd.print(":");
  print2Digits(ch_s);
}

// === Réglage heure ===
void reglerHeureMode(bool haut, bool reset, bool modeBtn) {
  static bool init = false;
  if (!init) {
    int h,m,s;
    lireHeureRTC(h,m,s);
    reglageHeure = h;
    reglageMinute = m;
    reglerHeure = true;
    init = true;
  }

  if (haut == LOW && lastHautState == HIGH) {
    if (reglerHeure) reglageHeure = (reglageHeure + 1) % 24;
    else reglageMinute = (reglageMinute + 1) % 60;
    beep();
    delay(150);
  }

  if (reset == LOW && lastResetState == HIGH) {
    reglerHeure = !reglerHeure; // Changement de réglage (Heure <-> Minute)
    beep();
    delay(150);
  }

  if (modeBtn == LOW && lastModeState == HIGH) {
    reglerHeureRTC(reglageHeure, reglageMinute, 0); // Sauvegarde
    beep();
    delay(250);
    init = false;
    mode = HORLOGE; // Retourne au mode horloge
    lcd.clear();
  }

  lcd.setCursor(0,0); lcd.print("Mode REGLAGE:");
  lcd.setCursor(4,1);
  // Affiche un > pour indiquer la valeur en cours de réglage
  if (reglerHeure) {
    lcd.print(">");
    print2Digits(reglageHeure);
    lcd.print(":");
    print2Digits(reglageMinute);
  } else {
    print2Digits(reglageHeure);
    lcd.print(":");
    lcd.print(">");
    print2Digits(reglageMinute);
  }
}

// === Fonctions utilitaires RTC ===
byte bcdToDec(byte val) { return (val / 16 * 10) + (val % 16); }
byte decToBcd(byte val) { return (val / 10 * 16) + (val % 10); }

void lireHeureRTC(int &h, int &m, int &s) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(DS1307_ADDRESS, 3);
  s = bcdToDec(Wire.read());
  m = bcdToDec(Wire.read());
  h = bcdToDec(Wire.read() & 0b00111111);
}

void reglerHeureRTC(int h, int m, int s) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.endTransmission();
}

void print2Digits(int val) {
  if (val < 10) lcd.print("0");
  lcd.print(val);
}

void beep() {
  digitalWrite(BUZ, HIGH);
  delay(60);
  digitalWrite(BUZ, LOW);
}