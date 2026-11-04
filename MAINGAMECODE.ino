#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 8
#define OLED_SCL 9

// --- CẤU HÌNH CHÂN NÚT BẤM ---
#define BTN_UP    5    
#define BTN_DOWN  4    
#define BTN_SKILL 6    

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Preferences preferences;

// --- TRẠNG THÁI GAME & ĐỘ KHÓ ---
enum GameState { TITLE, SETTINGS, PLAYING, PAUSED, PLAYER_DYING, GAME_OVER, FAKE_DEATH_SEQ, BUFF_MENU_CAT, BUFF_MENU_ITEM };
GameState gameState = TITLE;

enum Difficulty { EASY, NORMAL, HARD };
Difficulty currentDiff = NORMAL;
int titleMenuIndex = 0; // 0: PLAY, 1: SETTINGS

// --- THÔNG SỐ NGƯỜI CHƠI & BUFF HỆ THỐNG ---
float playerY = 28;
float playerHP = 8.0;     
float maxHP = 8.0;        
int score = 0;
int bestScore = 0;
int currentWave = 1;

// Buff stats
float fireRateMult = 1.0;
float bulletDamageBonus = 0.0;
float doubleShotChance = 0.0;
int maxPierces = 0;
float critChance = 0.0;
bool hasPassiveShield = false;
unsigned long passiveShieldCooldownEnd = 0;
float moveSpeedMult = 1.0;
float damageReduction = 0.0;

// Menu Buff
int menuCatIndex = 0;
int menuItemIndex = 0;

const char* attBuffs[] = {
  "Overclock (+15%Spd)", 
  "Hi-Caliber (+1.5Dmg)", 
  "Dbl Shot (+30%Chc)", 
  "Pierce (+1 Pierce)", 
  "Crit Core (+10%Crit)"
};

const char* defBuffs[] = {
  "Reinforced (+5 HP)", 
  "Nano Repair (+10 HP)", 
  "EnergyShield (30sCD)", 
  "Thruster (+15%Spd)", 
  "Armor (+10% DmgRed)"
};

// --- HỆ THỐNG NĂNG LƯỢNG & NỘ ---
float energy = 100.0;          
const float MAX_ENERGY = 100.0;
float ultiPercent = 0.0;       
const float MAX_ULTI = 100.0;
float bossHitUltiGained = 0.0; 
unsigned long lastEnergyRegen = 0;

// --- TRẠNG THÁI HIỆU ỨNG SKILL & ULTI ---
bool isLazeActive = false; unsigned long lazeActiveEnd = 0;
bool isUltiActive = false; unsigned long ultiActiveEnd = 0;
bool isEmpActive = false; unsigned long empActiveEnd = 0; unsigned long slowCooldownEnd = 0;
bool isChainActive = false; unsigned long chainActiveEnd = 0; unsigned long chainCooldownEnd = 0; unsigned long lastChainDamage = 0;
bool isBlackHoleActive = false; unsigned long blackHoleStart = 0; unsigned long blackHoleEnd = 0; unsigned long blackHoleCooldownEnd = 0;

// --- BLAST BOMB FX VARS ---
bool isBlastBombActive = false; unsigned long blastBombEnd = 0;

// --- HIỆU ỨNG GAME OVER ---
unsigned long deathStartTime = 0;
struct Particle { float x, y, vx, vy; bool active; };
#define MAX_PARTICLES 12 
Particle particles[MAX_PARTICLES];

// --- BUFF/ITEMS ---
bool hasShield = false;
bool isRapidFire = false; unsigned long rapidFireEnd = 0;
bool isTripleBullet = false; unsigned long tripleBulletEnd = 0;

// --- GIỮ NÚT SKILL & TRIPLE CLICK ---
unsigned long skillPressStart = 0; bool isSkillPressed = false; bool prevSkillRaw = false;
float currentHoldSec = 0.0; unsigned long lastHealTime = 0;
int skillClickCount = 0; unsigned long lastSkillClickTime = 0;
static bool prevUp = false, prevDown = false, prevSkillRawMenu = false;

// --- ITEMS DROP ---
enum ItemType { ITEM_NONE, ITEM_SHIELD, ITEM_RAPID, ITEM_TRIPLE };
struct Item { float x, y; ItemType type; bool active; };
#define MAX_ITEMS 3
Item items[MAX_ITEMS];

// --- QUÁI THƯỜNG ---
struct Enemy { float x, y, speed; bool active; };
#define MAX_ENEMIES 3
Enemy enemies[MAX_ENEMIES];
int enemiesDefeatedInWave = 0;

// --- BOSS HỆ THỐNG ---
bool bossActive = false; int bossType = 1;
float bossHp = 30; float bossMaxHp = 30;
float bossY = 20; float bossDirY = 1.0; float bossX = 140;

// Boss Vars
bool boss3Visible = true; unsigned long boss3StealthTimer = 0;
bool bossWarning = false; unsigned long bossWarningStart = 0; int pendingBossType = 6;
bool boss6Dash = false; float bossDashSpeed = 6.5; unsigned long boss6LastSpreadShot = 0;

// Boss 7 (Omega Core) Vars
int boss7Phase = 1; unsigned long boss7Timer = 0;
int boss7Volley = 0; bool boss7Resting = false;
int boss7Count = 0; float boss7SpeedMult = 1.0;
unsigned long seqStart = 0; const char* dataStrings[] = {"0101", "1100", "0011", "1010"};

enum LaserState { IDLE, CHARGING, FIRING };
LaserState laserState = IDLE; unsigned long laserTimer = 0; int laserY = 0;

// --- RUNG MÀN HÌNH ---
unsigned long shakeEndTime = 0; int shakeMagnitude = 0;
void triggerShake(int durationMs, int magnitude = 1) { shakeEndTime = millis() + durationMs; shakeMagnitude = magnitude; }

// --- ĐẠN ---
struct Bullet { float x, y, vx, vy, damage; bool active, isEnemy; int type; int pierce; };
#define MAX_BULLETS 155 
Bullet bullets[MAX_BULLETS];
unsigned long lastShootTime = 0;

// Khai báo hàm
void addUlti(float amount);
void spawnEnemy(int i);
void spawnItem(float x, float y);
void handleBossDeath();
void clearAllEnemyBullets();

void triggerPlayerDeath() {
  gameState = PLAYER_DYING; deathStartTime = millis();
  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].x = 6.0; particles[i].y = playerY + 4.0;
    particles[i].vx = (random(-20, 20) / 10.0); particles[i].vy = (random(-20, 20) / 10.0);
    particles[i].active = true;
  }
}

void takeDamageWithShields(float dmg) {
  if (hasShield) { hasShield = false; return; }
  if (hasPassiveShield && millis() > passiveShieldCooldownEnd) {
    passiveShieldCooldownEnd = millis() + 30000; triggerShake(300, 2); return;
  }
  float actualDmg = dmg * (1.0 - damageReduction);
  if (actualDmg < 0.1f) actualDmg = 0.1f;
  playerHP -= actualDmg;
  if (playerHP <= 0) triggerPlayerDeath();
}

void spawnItem(float x, float y) {
  if (random(0, 100) < 18) {
    for (int i = 0; i < MAX_ITEMS; i++) {
      if (!items[i].active) {
        items[i].x = x; items[i].y = y;
        int r = random(0, 3);
        if (r == 0) items[i].type = ITEM_SHIELD; else if (r == 1) items[i].type = ITEM_RAPID; else items[i].type = ITEM_TRIPLE;
        items[i].active = true; break;
      }
    }
  }
}

void spawnPlayerBullet(float x, float y, float vx, float vy, float dmg, int p) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].x = x; bullets[i].y = y; bullets[i].vx = vx; bullets[i].vy = vy;
      bullets[i].damage = dmg; bullets[i].active = true; bullets[i].isEnemy = false;
      bullets[i].type = 0; bullets[i].pierce = p; break;
    }
  }
}

void firePlayerBullets(float x, float y) {
  bool crit = random(0, 100) < (critChance * 100.0);
  float dmg = (1.0 + bulletDamageBonus) * (crit ? 2.0 : 1.0);
  if (isTripleBullet) {
    float vys[3] = {-1.2, 0.0, 1.2};
    for (int b = 0; b < 3; b++) spawnPlayerBullet(x, y, 4.8, vys[b], dmg, maxPierces);
  } else {
    for (int side = -2; side <= 2; side += 4) spawnPlayerBullet(x, y + side, 5.0, 0, dmg, maxPierces);
  }
  if (random(0, 100) < (doubleShotChance * 100.0)) {
    spawnPlayerBullet(x, y, 5.0, -0.6, dmg, maxPierces); spawnPlayerBullet(x, y, 5.0, 0.6, dmg, maxPierces);
  }
}

void fireEnemyBullet(float x, float y, float vx, float vy, float dmg = 1.0, int type = 0) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].x = x; bullets[i].y = y; bullets[i].vx = vx; bullets[i].vy = vy;
      bullets[i].damage = dmg; bullets[i].active = true; bullets[i].isEnemy = true;
      bullets[i].type = type; bullets[i].pierce = 0; break;
    }
  }
}

void spawnEnemy(int i) {
  enemies[i].x = SCREEN_WIDTH + random(10, 50) + (i * 15); 
  enemies[i].y = random(5, SCREEN_HEIGHT - 12);
  
  float speedBase = random(18, 28) / 10.0;
  if (currentDiff == EASY) speedBase *= 0.7f;      
  else if (currentDiff == HARD) speedBase *= 1.3f; 
  
  enemies[i].speed = speedBase; 
  enemies[i].active = true;
}

void startBoss(int type) {
  bossActive = true; bossType = type; bossY = 20; bossHitUltiGained = 0.0; laserState = IDLE; laserTimer = millis(); bossX = 105;
  int loopBonus = ((currentWave - 1) / 5) * 35;
  
  if (type == 1) bossMaxHp = 80 + loopBonus;
  else if (type == 2) bossMaxHp = 55 + loopBonus;
  else if (type == 3) { bossMaxHp = 60 + loopBonus; boss3StealthTimer = millis(); boss3Visible = true; }
  else if (type == 4) bossMaxHp = 65 + loopBonus;
  else if (type == 5) bossMaxHp = 70 + loopBonus;
  else if (type == 6) { bossMaxHp = 350 + (loopBonus * 3); boss6LastSpreadShot = millis(); boss6Dash = false; }
  else if (type == 7) { 
    boss7Count++; 
    boss7SpeedMult = 1.0 + (boss7Count - 1) * 0.15;
    bossMaxHp = 500.0 * (1.0 + (boss7Count - 1) * 0.25);
    boss7Phase = 1; boss7Timer = millis(); boss7Resting = false;
  }
  
  if (currentDiff == EASY) bossMaxHp *= 0.7f;
  else if (currentDiff == HARD) bossMaxHp *= 1.5f;

  bossHp = bossMaxHp; bossWarning = false; 
}

// LOGIC CẬP NHẬT: Wave 6, 7 và mỗi 12 wave (12, 24, 36, 48...)
bool isBuffWave(int w) { 
  return (w == 6 || w == 7 || (w > 0 && w % 12 == 0)); 
}

void handleBossDeath() {
  if (bossType == 7 && boss7Phase == 1) { gameState = FAKE_DEATH_SEQ; seqStart = millis(); bossHp = 1; return; }
  bossActive = false; score += (bossType == 7) ? 200 : 50;
  if (playerHP < maxHP) { playerHP += 2.0; if (playerHP > maxHP) playerHP = maxHP; }
  addUlti(20.0);
  for (int i=0; i<MAX_BULLETS; i++) bullets[i].active = false;
  if (isBuffWave(currentWave)) { gameState = BUFF_MENU_CAT; menuCatIndex = 0; } 
  else { currentWave++; }
}

void clearAllEnemyBullets() { for (int i = 0; i < MAX_BULLETS; i++) if (bullets[i].active && bullets[i].isEnemy) bullets[i].active = false; }
void addUlti(float amount) { if (!isUltiActive) { ultiPercent += amount; if (ultiPercent > MAX_ULTI) ultiPercent = MAX_ULTI; } }

void triggerBlastBomb() {
  isBlastBombActive = true; blastBombEnd = millis() + 500; triggerShake(500, 3);
  clearAllEnemyBullets();
  if (!bossActive) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (enemies[i].active) {
        spawnItem(enemies[i].x, enemies[i].y); spawnEnemy(i); score += 10; enemiesDefeatedInWave++; addUlti(0.5);
      }
    }
  }
  if (bossActive) {
    bossHp -= (bossMaxHp * 0.15f); if (bossHp <= 0) handleBossDeath();
  }
}

void applyBuff(int cat, int item) {
  if (cat == 0) {
    if (item == 0) fireRateMult += 0.15;
    else if (item == 1) bulletDamageBonus += 1.5;
    else if (item == 2) doubleShotChance += 0.30;
    else if (item == 3) maxPierces += 1;
    else if (item == 4) critChance += 0.10;
  } 
  else if (cat == 1) {
    if (item == 0) { maxHP += 5.0; playerHP += 5.0; }
    else if (item == 1) { playerHP += 10.0; if (playerHP > maxHP) playerHP = maxHP; }
    else if (item == 2) hasPassiveShield = true;
    else if (item == 3) moveSpeedMult += 0.15;
    else if (item == 4) damageReduction += 0.10;
  }
}

void resetGame() {
  playerY = 28; maxHP = 8.0; playerHP = 8.0; score = 0; currentWave = 1; energy = 100.0; ultiPercent = 0.0; bossActive = false; enemiesDefeatedInWave = 0;
  isLazeActive = false; isUltiActive = false; isEmpActive = false; isChainActive = false; isBlackHoleActive = false; isBlastBombActive = false;
  hasShield = false; isRapidFire = false; isTripleBullet = false; isSkillPressed = false;
  fireRateMult = 1.0; bulletDamageBonus = 0; doubleShotChance = 0.0; maxPierces = 0; critChance = 0.0;
  hasPassiveShield = false; passiveShieldCooldownEnd = 0; moveSpeedMult = 1.0; damageReduction = 0.0;
  boss7Count = 0;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < MAX_ITEMS; i++) items[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) spawnEnemy(i);
}

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP); pinMode(BTN_SKILL, INPUT_PULLUP);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for (;;);
  Wire.setClock(800000); 
  preferences.begin("space_game", false);
  bestScore = preferences.getInt("best_score", 0);
  preferences.end();
  gameState = TITLE;
}

void loop() {
  unsigned long now = millis();
  bool isUpRaw = (digitalRead(BTN_UP) == LOW);
  bool isDownRaw = (digitalRead(BTN_DOWN) == LOW);
  bool isSkillRaw = (digitalRead(BTN_SKILL) == LOW);
  bool upPressed = isUpRaw && !prevUp;
  bool downPressed = isDownRaw && !prevDown;
  bool skillPressedMenu = isSkillRaw && !prevSkillRawMenu;
  prevUp = isUpRaw; prevDown = isDownRaw; prevSkillRawMenu = isSkillRaw;

  // --- MÀN HÌNH TITLE MENU ---
  if (gameState == TITLE) {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
    display.setCursor(37, 2); display.print(F("SPACE GAME"));
    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);
    
    display.setCursor(35, 22); if (titleMenuIndex == 0) display.print(F("> PLAY")); else display.print(F("  PLAY"));
    display.setCursor(35, 34); if (titleMenuIndex == 1) display.print(F("> SETTINGS")); else display.print(F("  SETTINGS"));
    
    display.setCursor(20, 52); display.print(F("MODE: "));
    if (currentDiff == EASY) display.print(F("EASY"));
    else if (currentDiff == NORMAL) display.print(F("NORMAL"));
    else display.print(F("HARD"));
    display.display();

    if (upPressed || downPressed) titleMenuIndex = 1 - titleMenuIndex;
    if (skillPressedMenu) {
      if (titleMenuIndex == 0) { resetGame(); gameState = PLAYING; } else { gameState = SETTINGS; }
    }
    return;
  }

  // --- MÀN HÌNH SETTINGS ---
  if (gameState == SETTINGS) {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
    display.setCursor(28, 0); display.print(F("- SETTINGS -"));
    display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    display.setCursor(20, 15); display.print(F("MODE: < "));
    if (currentDiff == EASY) display.print(F("EASY"));
    else if (currentDiff == NORMAL) display.print(F("NORMAL"));
    else if (currentDiff == HARD) display.print(F("HARD"));
    display.print(F(" >"));
    display.drawFastHLine(10, 26, 108, SSD1306_WHITE);

    if (currentDiff == EASY) {
      display.setCursor(10, 31); display.print(F("* Boss -30% HP"));
      display.setCursor(10, 41); display.print(F("*  SLOWER ENEMY"));
      display.setCursor(10, 51); display.print(F("* GOOD FOR NEWBIE"));
    } else if (currentDiff == NORMAL) {
      display.setCursor(10, 35); display.print(F("* BALANCE"));
      display.setCursor(10, 47); display.print(F("* NORMAL BOSS"));
    } else if (currentDiff == HARD) {
      display.setCursor(0, 30); display.print(F("* Boss+50%HP|Enemy n"));
      display.setCursor(0, 40); display.print(F("* FASTER BULLETS"));
      display.setCursor(0, 50); display.print(F("* BUFF Boss 7!"));
    }
    display.display();

    if (upPressed) {
      if (currentDiff == EASY) currentDiff = HARD; else if (currentDiff == NORMAL) currentDiff = EASY; else currentDiff = NORMAL;
    }
    if (downPressed) {
      if (currentDiff == EASY) currentDiff = NORMAL; else if (currentDiff == NORMAL) currentDiff = HARD; else currentDiff = EASY;
    }
    if (skillPressedMenu) gameState = TITLE;
    return;
  }

  // --- MÀN HÌNH CHỌN BUFF ---
  if (gameState == BUFF_MENU_CAT || gameState == BUFF_MENU_ITEM) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
    if (gameState == BUFF_MENU_CAT) {
      display.setCursor(16, 2); display.print(F("BOSS DEFEATED!"));
      display.drawFastHLine(0, 13, 128, SSD1306_WHITE);
      display.setCursor(20, 20); display.print(F("Select Category:"));
      display.setCursor(25, 36); if (menuCatIndex == 0) display.print(F("> ")); else display.print(F("  ")); display.print(F("ATTACK UPGRADES"));
      display.setCursor(25, 48); if (menuCatIndex == 1) display.print(F("> ")); else display.print(F("  ")); display.print(F("DEFENSE UPGRADES"));
      if (upPressed || downPressed) menuCatIndex = 1 - menuCatIndex;
      if (skillPressedMenu) { gameState = BUFF_MENU_ITEM; menuItemIndex = 0; }
    } else {
      display.setCursor(0, 0); display.print(menuCatIndex == 0 ? F("-- ATTACK BUFFS --") : F("-- DEFENSE BUFFS --"));
      for (int i = 0; i < 5; i++) {
        int yPos = 12 + (i * 10); display.setCursor(0, yPos);
        if (menuItemIndex == i) display.print(F(">")); else display.print(F(" "));
        display.setCursor(8, yPos); display.print(menuCatIndex == 0 ? attBuffs[i] : defBuffs[i]);
      }
      if (upPressed) { menuItemIndex--; if(menuItemIndex < 0) menuItemIndex = 4; }
      if (downPressed) { menuItemIndex++; if(menuItemIndex > 4) menuItemIndex = 0; }
      if (skillPressedMenu) { applyBuff(menuCatIndex, menuItemIndex); currentWave++; gameState = PLAYING; }
    }
    display.display(); return;
  }

  // --- CẢNH BÁO BOSS ---
  if (bossWarning) {
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(2); display.setCursor(22, 10); display.print(F("WARNING"));
    if ((now / 200) % 2 == 0) { display.setTextSize(3); display.setCursor(56, 34); display.print(F("!")); }
    if (now < shakeEndTime) display.ssd1306_command(SSD1306_SETSTARTLINE | (random(-shakeMagnitude, shakeMagnitude + 1) & 0x3F));
    else display.ssd1306_command(SSD1306_SETSTARTLINE | 0);
    display.display();
    if (now - bossWarningStart > 2500) {
      bossWarning = false; startBoss(pendingBossType);
      if (pendingBossType == 6) { boss6Dash = true; bossX = 140; }
    }
    return;
  }

  // --- FAKE DEATH BOSS 7 SCRIPT ---
  if (gameState == FAKE_DEATH_SEQ) {
    unsigned long elapsed = now - seqStart;
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(1);
    if (elapsed < 2000) { display.setCursor(8, 30); display.print(F("MISSION COMPLETE...")); }
    else if (elapsed < 4000) { /* Black Screen */ }
    else if (elapsed < 6000) { display.setCursor(20, 30); display.print(F("SYSTEM ERROR...")); }
    else if (elapsed < 8000) { 
      display.setCursor(15, 30); display.print(F("MEMORY CORRUPTED")); 
      if((elapsed/100)%2==0) display.fillRect(random(0,128), random(0,64), random(10,30), 2, SSD1306_WHITE);
    }
    else if (elapsed < 10000) { /* Boss Hides */ }
    else if (elapsed < 12000) {
      display.setCursor(15, 25); display.print(F("REBOOTING CORE..."));
      display.setCursor(10, 40); display.print(F("["));
      int p = map(elapsed, 10000, 12000, 0, 10);
      for(int k=0; k<10; k++) { if(k<p) display.print(F("#")); else display.print(F(" ")); }
      display.print(F("] 100%"));
    }
    else if (elapsed < 14000) { display.setCursor(0, 30); display.print(F("UNKNOWN ENTITY DETECTED")); }
    else { gameState = PLAYING; boss7Phase = 3; bossHp = bossMaxHp; boss7Timer = now; boss7Volley = 0; boss7Resting = false; }
    display.display(); return;
  }

  if (gameState == PLAYER_DYING) {
    display.clearDisplay(); unsigned long elapsed = now - deathStartTime;
    if ((elapsed / 80) % 2 == 0 && elapsed < 800) display.fillTriangle(2, (int)playerY, 2, (int)playerY+8, 10, (int)playerY+4, SSD1306_WHITE);
    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (particles[i].active) { particles[i].x += particles[i].vx; particles[i].y += particles[i].vy; display.drawPixel((int)particles[i].x, (int)particles[i].y, SSD1306_WHITE); }
    }
    display.display(); if (elapsed >= 1200) gameState = GAME_OVER; return;
  }

  if (gameState == GAME_OVER) {
    if (score > bestScore) { bestScore = score; preferences.begin("space_game", false); preferences.putInt("best_score", bestScore); preferences.end(); }
    display.clearDisplay(); display.setTextColor(SSD1306_WHITE); display.setTextSize(2);
    display.setCursor(10, 10); display.println(F("GAME OVER"));
    display.setTextSize(1); display.setCursor(25, 32); display.print(F("SCORE: ")); display.print(score);
    display.setCursor(5, 50); display.print(F("BAM NUT BAT KI DE RESTART")); display.display();
    if (isUpRaw || isDownRaw || isSkillRaw) { delay(300); gameState = TITLE; } return;
  }

  // --- LOGIC CHÍNH TRONG GAME ---
  if (now - lastEnergyRegen >= 1000) { energy += 3.5; if (energy > MAX_ENERGY) energy = MAX_ENERGY; lastEnergyRegen = now; }

  if (isUpRaw && isDownRaw && !isSkillRaw && (now - lastHealTime > 12000)) {
    if (playerHP < maxHP) { playerHP += 1.5; if (playerHP > maxHP) playerHP = maxHP; lastHealTime = now; }
  }

  if (isUpRaw && isDownRaw && isSkillRaw) {
    if (!isChainActive && now > chainCooldownEnd && energy >= 100.0) {
      energy -= 100.0; isChainActive = true; chainActiveEnd = now + 5000; chainCooldownEnd = now + 15000; isSkillPressed = false; triggerShake(5000, 1); 
    }
  }

  if (isSkillRaw && !prevSkillRaw) { 
    if (now - lastSkillClickTime > 500) skillClickCount = 1; else skillClickCount++;
    lastSkillClickTime = now;
    if (skillClickCount == 3) {
      if (!isEmpActive && now > slowCooldownEnd && ultiPercent >= 25.0) {
        ultiPercent -= 25.0; playerHP += 2.0; if (playerHP > maxHP) playerHP = maxHP;
        isEmpActive = true; empActiveEnd = now + 10000; slowCooldownEnd = now + 18000; skillClickCount = 0; 
      }
    }
  }
  prevSkillRaw = isSkillRaw;

  if (isSkillRaw) {
    if (!isSkillPressed) { isSkillPressed = true; skillPressStart = now; }
    currentHoldSec = (now - skillPressStart) / 1000.0;
  } else {
    if (isSkillPressed) {
      float holdDuration = (now - skillPressStart) / 1000.0; isSkillPressed = false;
      if (isUltiActive && holdDuration >= 6.0) {
        if (now > blackHoleCooldownEnd && !isBlackHoleActive) {
          isBlackHoleActive = true; blackHoleStart = now; blackHoleEnd = now + 5000; blackHoleCooldownEnd = now + 20000; triggerShake(5000, 2); 
        }
      }
      else if (holdDuration >= 8.0) { if (ultiPercent >= 100.0) { isUltiActive = true; ultiActiveEnd = now + 30000; ultiPercent = 0.0; } }
      else if (holdDuration >= 5.0) { if (energy >= 50.0) { energy -= 50.0; triggerBlastBomb(); } }
      else if (holdDuration >= 3.0) { if (energy >= 25.0) { energy -= 25.0; isLazeActive = true; lazeActiveEnd = now + 3500; } }
      else if (holdDuration >= 1.0) { if (gameState == PLAYING) gameState = PAUSED; else if (gameState == PAUSED) gameState = PLAYING; }
      currentHoldSec = 0.0;
    }
  }

  if (gameState == PAUSED) {
    display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE); display.setCursor(28, 25); display.print(F("PAUSED")); display.display(); return;
  }

  if (isLazeActive && now > lazeActiveEnd) isLazeActive = false;
  if (isUltiActive && now > ultiActiveEnd) isUltiActive = false;
  if (isEmpActive && now > empActiveEnd) isEmpActive = false;
  if (isChainActive && now > chainActiveEnd) isChainActive = false;
  if (isRapidFire && now > rapidFireEnd) isRapidFire = false;
  if (isTripleBullet && now > tripleBulletEnd) isTripleBullet = false;
  if (isBlastBombActive && now > blastBombEnd) isBlastBombActive = false;

  float timeScale = isEmpActive ? 0.15 : 1.0;

  if (isUpRaw && !isDownRaw && playerY > 0) playerY -= 2.0 * moveSpeedMult;
  if (isDownRaw && !isUpRaw && playerY < SCREEN_HEIGHT - 12) playerY += 2.0 * moveSpeedMult;

  if (!isEmpActive && !isBlackHoleActive) {
    int shootDelay = (isRapidFire ? 80 : 180) / fireRateMult;
    if (now - lastShootTime > shootDelay) {
      firePlayerBullets(12, playerY + 4);
      if (isUltiActive) { firePlayerBullets(12, playerY - 8); firePlayerBullets(12, playerY + 16); }
      lastShootTime = now;
    }
  }

  if (isBlackHoleActive) {
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (enemies[i].active) {
        enemies[i].x += (64.0 - enemies[i].x) * 0.15; enemies[i].y += (32.0 - enemies[i].y) * 0.15;
        if (abs(enemies[i].x - 64) < 6 && abs(enemies[i].y - 32) < 6) { spawnItem(enemies[i].x, enemies[i].y); spawnEnemy(i); score += 10; enemiesDefeatedInWave++; }
      }
    }
    playerY += (28.0 - playerY) * 0.12;
    if (bossActive) { bossY += (22.0 - bossY) * 0.12; bossX += (64.0 - bossX) * 0.12; }

    if (now > blackHoleEnd) {
      isBlackHoleActive = false; playerY = 28;
      if (bossActive) {
        bossX = 105; bossY = 20; bossHp -= (bossMaxHp * 0.35f); score += 50;
        if (bossHp <= 0) handleBossDeath();
      }
      for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) { spawnItem(enemies[i].x, enemies[i].y); spawnEnemy(i); score += 10; enemiesDefeatedInWave++; }
      }
    }
  }

  if (isChainActive && (now - lastChainDamage > 100)) {
    lastChainDamage = now;
    if (!bossActive) {
      for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) { spawnItem(enemies[i].x, enemies[i].y); spawnEnemy(i); score += 10; enemiesDefeatedInWave++; addUlti(0.5); }
      }
    } else {
      bossHp -= ((bossMaxHp * 0.45f) / 50.0f); score += 3;
      if (bossHp <= 0) handleBossDeath();
    }
  }

  for (int i = 0; i < MAX_ITEMS; i++) {
    if (items[i].active) {
      items[i].x -= (1.2 * timeScale);
      if (items[i].x <= 14 && items[i].x >= 0 && abs(items[i].y - playerY) < 12) {
        if (items[i].type == ITEM_SHIELD) hasShield = true;
        else if (items[i].type == ITEM_RAPID) { isRapidFire = true; rapidFireEnd = now + 8000; }
        else if (items[i].type == ITEM_TRIPLE) { isTripleBullet = true; tripleBulletEnd = now + 8000; }
        items[i].active = false;
      }
      if (items[i].x < -6) items[i].active = false;
    }
  }

  if (!bossActive && !isBlackHoleActive) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (enemies[i].active) {
        enemies[i].x -= (enemies[i].speed * timeScale);
        if (enemies[i].x <= 10 && enemies[i].x >= 2 && abs(enemies[i].y - playerY) < 8) { takeDamageWithShields(1.0); spawnEnemy(i); }
        if (enemies[i].x < -8) spawnEnemy(i);
        int enemyShootChance = (currentDiff == HARD) ? 6 : 4;
        if (random(0, 100) < (enemyShootChance * timeScale)) fireEnemyBullet(enemies[i].x, enemies[i].y + 3, -3.2, 0, 1.0);
      }
    }
    if (enemiesDefeatedInWave >= 35) {
      enemiesDefeatedInWave = 0;
      int type;
      // CẬP NHẬT: Boss 7 xuất hiện ở mốc 12 wave (thay vì 15)
      if (currentWave >= 7 && (currentWave - 7) % 12 == 0) type = 7;
      else type = ((currentWave - 1) % 6) + 1;
      
      if (type == 6 || type == 7) { bossWarning = true; bossWarningStart = now; pendingBossType = type; triggerShake(4000, 1); } 
      else startBoss(type);
    }
  }

  if (bossActive && !isBlackHoleActive) {
    if (bossType == 7) {
      if (boss7Phase == 1) {
        bossY += (bossDirY * 1.5 * timeScale * boss7SpeedMult);
        if (bossY <= 5 || bossY >= SCREEN_HEIGHT - 25) bossDirY = -bossDirY;
        
        int boss7ShootDelay = (currentDiff == HARD) ? 200 : 280;
        if (now % (int)(boss7ShootDelay / timeScale / boss7SpeedMult) < 20) fireEnemyBullet(105, bossY + 10, -4.0 * boss7SpeedMult, 0, 1.0);
        
        if (laserState == IDLE && now - laserTimer > (5000 / boss7SpeedMult)) { laserState = CHARGING; laserTimer = now; laserY = bossY + 10; }
        else if (laserState == CHARGING && now - laserTimer > 1000) { laserState = FIRING; laserTimer = now; }
        else if (laserState == FIRING) {
          triggerShake(100, 1);
          if (now - laserTimer > 800) { laserState = IDLE; laserTimer = now; }
          if (playerY + 8 >= laserY - 6 && playerY <= laserY + 6) takeDamageWithShields(0.2);
        }
      } 
      else if (boss7Phase == 3) {
        float currentSpeed = boss7Resting ? (1.2 * boss7SpeedMult) : (2.0 * boss7SpeedMult);
        bossY += bossDirY * currentSpeed * timeScale;
        if (bossY <= 2) { bossY = 2; bossDirY = 1.0; } else if (bossY >= SCREEN_HEIGHT - 22) { bossY = SCREEN_HEIGHT - 22; bossDirY = -1.0; }
        
        int phase3RestDelay = (currentDiff == HARD) ? 1500 : 2000;
        if (boss7Resting) {
          if (now - boss7Timer > (phase3RestDelay / boss7SpeedMult)) { boss7Resting = false; boss7Volley = 0; boss7Timer = now; }
        } else {
          int phase3ShootDelay = (currentDiff == HARD) ? 150 : 200;
          if (now - boss7Timer > (phase3ShootDelay / boss7SpeedMult)) {
            boss7Timer = now;
            fireEnemyBullet(105, bossY + 8, -4.8 * boss7SpeedMult, 0, 2.0, random(1, 5));
            boss7Volley++;
            if (boss7Volley >= 5) { boss7Resting = true; boss7Timer = now; }
          }
        }
      }
    }
    else {
      if (bossType == 6 && boss6Dash) {
        bossX -= (bossDashSpeed * timeScale);
        if (bossX <= 105) { bossX = 105; boss6Dash = false; }
        if (bossX <= 12) { takeDamageWithShields(99.0); boss6Dash = false; }
      }

      bool shouldMove = !((bossType == 5 || bossType == 6) && laserState != IDLE);
      if (shouldMove) { bossY += (bossDirY * 1.2 * timeScale); if (bossY <= 2 || bossY >= SCREEN_HEIGHT - 22) bossDirY = -bossDirY; }
      if (bossType == 3) { if (now - boss3StealthTimer > 2500) { boss3Visible = !boss3Visible; boss3StealthTimer = now; } } else boss3Visible = true;
      
      int bossNormalShootDelay = (currentDiff == HARD) ? 200 : 300;
      if (now % (int)(bossNormalShootDelay / timeScale) < 20) {
        if (bossType == 1) fireEnemyBullet(105, bossY + 10, -3.5, 0, 1.5);
        else if (bossType == 2) { fireEnemyBullet(105, bossY + 4, -3.8, 0, 1.0); fireEnemyBullet(105, bossY + 16, -3.8, 0, 1.0); }
        else if (bossType == 3 && boss3Visible) fireEnemyBullet(105, bossY + 10, -3.5, 0, 1.0);
        else if (bossType == 4) { fireEnemyBullet(105, bossY + 10, -3.2, 0, 1.0); fireEnemyBullet(105, bossY + 10, -3.0, -1.5, 1.0); fireEnemyBullet(105, bossY + 10, -3.0, 1.5, 1.0); }
      }
      
      int boss6SpreadDelay = (currentDiff == HARD) ? 1000 : 1400;
      if (bossType == 6 && !boss6Dash && (now - boss6LastSpreadShot > (boss6SpreadDelay / timeScale))) {
        float dx = 6.0 - bossX; float dy = playerY - (bossY + 10); float angle = atan2(dy, dx);
        for (int i = -2; i <= 2; i++) fireEnemyBullet(bossX, bossY + 10, cos(angle + radians(i * 10.0)) * 3.0, sin(angle + radians(i * 10.0)) * 3.0, 1.0);
        boss6LastSpreadShot = now;
      }
      
      if (bossType == 5 || bossType == 6) {
        if (laserState == IDLE) {
          if (now - laserTimer > (bossType == 5 ? 2800 : 3200)) { laserState = CHARGING; laserTimer = now; laserY = bossY + 10; }
        } else if (laserState == CHARGING) {
          if (now - laserTimer > (bossType == 5 ? 1200 : 800)) { laserState = FIRING; laserTimer = now; }
        } else if (laserState == FIRING) {
          triggerShake(100, (bossType == 5 ? 1 : 2));
          if (now - laserTimer > (bossType == 5 ? 1000 : 400)) { laserState = IDLE; laserTimer = now; }
          if (playerY + 8 >= laserY - ((bossType == 5) ? 5 : 8) && playerY <= laserY + ((bossType == 5) ? 5 : 8)) takeDamageWithShields(bossType == 5 ? 0.15 : 0.2);
        }
      }
    }
  }

  if (isLazeActive) {
    float lazeY = playerY + 3;
    if (!bossActive) {
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].active && lazeY >= enemies[e].y - 2 && lazeY <= enemies[e].y + 8) { spawnItem(enemies[e].x, enemies[e].y); spawnEnemy(e); score += 10; enemiesDefeatedInWave++; addUlti(0.4); }
      }
    }
    if (bossActive && boss3Visible && lazeY >= bossY - 2 && lazeY <= bossY + 22) { bossHp -= 1.0; score += 2; if (bossHp <= 0) handleBossDeath(); }
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].x += bullets[i].vx * (bullets[i].isEnemy ? timeScale : 1.0);
      bullets[i].y += bullets[i].vy * (bullets[i].isEnemy ? timeScale : 1.0);
      if (bullets[i].x < -20 || bullets[i].x > SCREEN_WIDTH + 20) bullets[i].active = false;

      if (bullets[i].isEnemy && bullets[i].x <= 10 && bullets[i].x >= 2 && bullets[i].y >= playerY && bullets[i].y <= playerY + 8) {
        bullets[i].active = false; takeDamageWithShields(bullets[i].damage);
      }

      if (!bullets[i].isEnemy && !bossActive) {
        for (int e = 0; e < MAX_ENEMIES; e++) {
          if (enemies[e].active && abs(bullets[i].x - enemies[e].x) < 6 && abs(bullets[i].y - enemies[e].y) < 6) {
            spawnItem(enemies[e].x, enemies[e].y); spawnEnemy(e); score += 10; enemiesDefeatedInWave++; addUlti(0.4);
            if (bullets[i].pierce > 0) bullets[i].pierce--; else bullets[i].active = false; break;
          }
        }
      }

      int drawBossX = (bossType == 6 || bossType == 7) ? (int)bossX : 105;
      if (!bullets[i].isEnemy && bossActive && boss3Visible && bullets[i].x >= drawBossX && bullets[i].y >= bossY && bullets[i].y <= bossY + 20) {
        bullets[i].active = false; bossHp -= bullets[i].damage; score += 15;
        if (bossHitUltiGained < 10.0) { addUlti(1.0); bossHitUltiGained += 1.0; }
        if (bossHp <= 0) handleBossDeath();
      }
    }
  }

  // --- VẼ MÀN HÌNH ---
  display.clearDisplay();
  display.fillTriangle(2, (int)playerY, 2, (int)playerY + 8, 10, (int)playerY + 4, SSD1306_WHITE);

  if (hasShield) display.drawCircle(6, (int)playerY + 4, 8, SSD1306_WHITE);
  if (hasPassiveShield && now > passiveShieldCooldownEnd) display.drawCircle(6, (int)playerY + 4, 12, SSD1306_WHITE);

  if (isUltiActive) {
    display.fillTriangle(2, (int)playerY - 10, 2, (int)playerY - 4, 8, (int)playerY - 7, SSD1306_WHITE);
    display.fillTriangle(2, (int)playerY + 12, 2, (int)playerY + 18, 8, (int)playerY + 15, SSD1306_WHITE);
  }
  if (isLazeActive) display.fillRect(10, (int)playerY + 3, 118, 3, SSD1306_WHITE);

  if (isBlackHoleActive) {
    display.fillCircle(64, 32, 4, SSD1306_WHITE);
    int pulseR = 14 + (int)(sin((now - blackHoleStart) * 0.015) * 4.0);
    display.drawCircle(64, 32, pulseR, SSD1306_WHITE);
    for (int p = 0; p < 8; p++) {
      float a = (now - blackHoleStart) * 0.012 + p * (3.14159 / 4.0);
      display.drawPixel(constrain((int)(64 + cos(a) * (pulseR + ((p%2==0)?3:-3))), 0, 127), constrain((int)(32 + sin(a) * (pulseR + ((p%2==0)?3:-3))), 0, 63), SSD1306_WHITE);
    }
  }

  if (isChainActive) {
    if (!bossActive) {
      for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
          int mX = (10 + (int)enemies[i].x) / 2 + random(-4, 4), mY = ((int)playerY + (int)enemies[i].y) / 2 + random(-4, 4);
          display.drawLine(10, (int)playerY+4, mX, mY, SSD1306_WHITE); display.drawLine(mX, mY, (int)enemies[i].x, (int)enemies[i].y, SSD1306_WHITE);
        }
      }
    } else if (boss3Visible) {
      for (int b = 0; b < 3; b++) {
        int mX = (10 + bossX) / 2 + random(-8, 8), mY = ((int)playerY + (int)bossY + 10) / 2 + random(-8, 8);
        display.drawLine(10, (int)playerY+4, mX, mY, SSD1306_WHITE); display.drawLine(mX, mY, bossX + 10, (int)bossY + 10, SSD1306_WHITE);
      }
    }
  }

  for (int i = 0; i < MAX_ITEMS; i++) {
    if (items[i].active) {
      display.drawCircle((int)items[i].x + 3, (int)items[i].y + 3, 4, SSD1306_WHITE);
      display.setCursor((int)items[i].x + 1, (int)items[i].y); display.setTextSize(1);
      display.print(items[i].type == ITEM_SHIELD ? F("S") : (items[i].type == ITEM_RAPID ? F("F") : F("T")));
    }
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      if (bullets[i].type >= 1 && bullets[i].type <= 4) {
        display.setTextSize(1); display.setCursor((int)bullets[i].x, (int)bullets[i].y - 3); display.print(dataStrings[bullets[i].type - 1]);
      } else {
        if (bullets[i].isEnemy) display.fillCircle((int)bullets[i].x, (int)bullets[i].y, 1, SSD1306_WHITE);
        else display.drawFastHLine((int)bullets[i].x, (int)bullets[i].y, 4, SSD1306_WHITE);
      }
    }
  }

  if (!bossActive) { for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) display.drawRect((int)enemies[i].x, (int)enemies[i].y, 6, 6, SSD1306_WHITE); }

  if (bossActive && boss3Visible) {
    int drawBossX = (bossType == 6 || bossType == 7) ? (int)bossX : 105;
    if (bossType == 7) {
      if (boss7Phase == 1) { display.drawRect(drawBossX, (int)bossY, 20, 20, SSD1306_WHITE); display.drawCircle(drawBossX + 10, (int)bossY + 10, 5, SSD1306_WHITE); } 
      else if (boss7Phase == 3) { 
        display.fillRect(drawBossX, (int)bossY, 20, 20, SSD1306_WHITE); display.fillCircle(drawBossX + 10, (int)bossY + 10, 6, SSD1306_BLACK); 
        if ((now/50)%2==0) display.drawFastHLine(drawBossX - 5, (int)bossY + random(0, 20), random(5, 15), SSD1306_WHITE);
      }
    } else {
      display.drawRect(drawBossX, (int)bossY, 20, 20, SSD1306_WHITE); display.fillRect(drawBossX + 5, (int)bossY + 6, 8, 8, SSD1306_WHITE);
    }
    if (bossType == 5 || bossType == 6 || (bossType == 7 && boss7Phase == 1)) {
      if (laserState == CHARGING && (now / 50) % 2 == 0) {
        for (int x = 0; x < drawBossX; x += 6) display.drawFastHLine(x, laserY, 3, SSD1306_WHITE); display.setCursor(drawBossX - 10, laserY - 8); display.print(F("!"));
      } else if (laserState == FIRING) { display.fillRect(0, laserY - ((bossType==6)?8:6), drawBossX, ((bossType==6)?16:12), SSD1306_WHITE); }
    }
    display.drawRect(35, 0, 50, 4, SSD1306_WHITE);
    int hpW = map((int)bossHp, 0, (int)bossMaxHp, 0, 48);
    if (hpW > 0) display.fillRect(36, 1, hpW, 2, SSD1306_WHITE);
  }

  // --- VẼ HIỆU ỨNG VIỀN MÀN HÌNH NỔ BOMB ---
  if (isBlastBombActive) {
    if ((now / 60) % 2 == 0) {
      display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
      display.drawRect(1, 1, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 2, SSD1306_WHITE);
    }
  }

  // UI DISPLAY
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0); display.print(F("HP:")); display.print(playerHP, 1);
  display.setCursor(95, 0); display.print(F("W:")); display.print(currentWave);
  display.setCursor(0, 56); display.print(F("E:")); display.print((int)energy);

  display.setCursor(70, 56);
  if (isBlackHoleActive) display.print(F("BLACKHOLE"));
  else if (isChainActive) display.print(F("LIGHTNING!"));
  else if (isEmpActive) display.print(F("SLOW!"));
  else { display.print(F("Uti:")); display.print((int)ultiPercent); display.print(F("%")); }

  if (isSkillPressed) { display.setCursor(35, 56); display.print(F("H:")); display.print(currentHoldSec, 1); display.print(F("s")); }

  if (now < shakeEndTime) display.ssd1306_command(SSD1306_SETSTARTLINE | (random(-shakeMagnitude, shakeMagnitude + 1) & 0x3F));
  else display.ssd1306_command(SSD1306_SETSTARTLINE | 0);

  display.display(); delay(15);
}