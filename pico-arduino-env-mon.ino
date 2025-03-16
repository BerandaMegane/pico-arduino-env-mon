// 必要な外部ライブラリ
// https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_GFX.h>  // install "Adafruit GFX Library"
// https://github.com/adafruit/Adafruit_ILI9341
#include <Adafruit_ILI9341.h>  // install "Adafruit ILI9341"
// https://github.com/malokhvii-eduard/arduino-bme280
#include <Bme280.h>  // install "Bme280" (by Eduard Malokhvii)

// フォント
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/Org_01.h>

// ピン接続
#define UART_TX    0  // UART TX   (MH-Z19C RX)
#define UART_RX    1  // UART RX   (MH-Z19C TX)
#define SPI_MISO  16  // SPI0 MISO (BME280, ILI9341)
#define BME280_CS 17  // SPI  CS   (BME280)
#define SPI_SCK   18  // SPI0 SCK  (BME280, ILI9341)
#define SPI_MOSI  19  // SPI0 MOSI (BME280, ILI9341)
#define LCD_DC    20  // ILI9341 DC
#define LCD_RST   21  // ILI9341 RST
#define LCD_CS    22  // SPI  CS   (ILITFT_9341)

// 液晶画面
#define LCD_WIDTH    320
#define LCD_HEIGHT   240
#define LCD_ROTATION   1  // 画面の回転 1 or 3
#define LCD_BLACK 0x0000
#define LCD_WHITE 0xFFFF

const int16_t SAMPLING_INTERVAL_MS = 1000;
const int16_t GRAPH_DEPTH = 200;  // グラフの横幅
const int16_t GRAPH_TIME_RANGE_H = 24;  // グラフの全期間の時間
const uint32_t GRAPH_SHIFT_INTERVAL_MS = (uint32_t)GRAPH_TIME_RANGE_H * 3600 * 1000 / GRAPH_DEPTH;

const float TEMPERATURE_OFFSET = -2.0;  // 温度補正値
Bme280FourWire bme280;
Adafruit_ILI9341 lcd = Adafruit_ILI9341(&SPI, LCD_DC, LCD_CS, LCD_RST);
GFXcanvas16 canvas(LCD_WIDTH, LCD_HEIGHT);

void drawInvertDottedVLine(int16_t x, int16_t y, int16_t h, int16_t invert_dot, int16_t not_invert_dot) {
  const int16_t length = invert_dot + not_invert_dot;
  int16_t i = 0;
  for (int16_t px_y = y; px_y < (y + h); px_y++) {
    if ((i % length) < invert_dot) {
      canvas.drawPixel(x, px_y, 0xFFFF - canvas.getPixel(x, px_y));
    }
    i++;
  }
}

void drawInvertDottedHLine(int16_t x, int16_t y, int16_t h, int16_t invert_dot, int16_t not_invert_dot) {
  const int16_t length = invert_dot + not_invert_dot;
  int16_t i = 0;
  for (int16_t px_x = x; px_x < (x + h); px_x++) {
    if ((i % length) < invert_dot) {
      canvas.drawPixel(px_x, y, 0xFFFF - canvas.getPixel(px_x, y));
    }
    i++;
  }
}

void drawDottedVLine(int16_t x, int16_t y, int16_t h, uint16_t color, int16_t invert_dot, int16_t not_invert_dot) {
  const int16_t length = invert_dot + not_invert_dot;
  int16_t i = 0;
  for (int16_t px_y = y; px_y < (y + h); px_y++) {
    if ((i % length) < invert_dot) {
      canvas.drawPixel(x, px_y, color);
    }
    i++;
  }
}

void drawDottedHLine(int16_t x, int16_t y, int16_t h, uint16_t color, int16_t invert_dot, int16_t not_invert_dot) {
  const int16_t length = invert_dot + not_invert_dot;
  int16_t i = 0;
  for (int16_t px_x = x; px_x < (x + h); px_x++) {
    if ((i % length) < invert_dot) {
      canvas.drawPixel(px_x, y, color);
    }
    i++;
  }
}

uint16_t get16bitColor(uint8_t r, uint8_t g, uint8_t b) {
  // 0b RRRR RGGG GGGB BBBB
  return r << 11 | g << 5 | b;
} 

uint16_t get16bitColor(float r, float g, float b) {
  uint8_t temp_r = r * 0b11111;
  uint8_t temp_g = g * 0b111111;
  uint8_t temp_b = b * 0b11111;
  return get16bitColor(temp_r, temp_g, temp_b);
}

class Graph {
public:
  // サンプル数
  static const int DEPTH = GRAPH_DEPTH;
  // グラフのサイズ
  static const int WIDTH = DEPTH;
  static const int HEIGHT = 60;
  // 垂直線の間隔
  static const int VERTICAL_LINE_STEP = WIDTH / 4;

  // グラフの位置
  const int left, top;
  // 最小の縦方向範囲
  const float min_range;
  // 最小値・最大値の履歴
  float min_log[DEPTH];
  float max_log[DEPTH];
  // 現在のデータ数
  int num_data = 0;
  // グラフ全体の最小値・最大値
  float total_min = 0;
  float total_max = 0;
  // 水平線の間隔
  float horizontal_line_step = 1;

  const uint16_t plot_color;
  const uint16_t fill_color;

  // コンストラクタ
  Graph(int x, int y, float min_range, uint16_t plot_color, uint16_t fill_color) : 
    left(x), top(y), min_range(min_range), plot_color(plot_color), fill_color(fill_color) { }

  // データを追加する関数
  void push(float value, bool shift) {
    if (num_data == 0) {
      // 最初のデータを追加
      num_data = 1;
      min_log[0] = max_log[0] = value;
    } else if (shift) {
      // 新しいデータを追加し、過去のデータを1つ後ろにシフト
      for (int i = DEPTH - 1; i >= 1; i--) {
        min_log[i] = min_log[i-1];
        max_log[i] = max_log[i-1];
      }
      min_log[0] = max_log[0] = value;
      if (num_data < DEPTH) {
        num_data++;
      }
    } else {
      // 最新のデータと比較し、最小・最大値を更新
      if (min_log[0] > value) min_log[0] = value;
      if (max_log[0] < value) max_log[0] = value;
    }
    
    // グラフ全体の最小・最大値を更新
    if (num_data == 0) {
      total_min = 0;
      total_max = 0;
    } else {
      total_min = min_log[0];
      total_max = max_log[0];
      for (int i = 1; i < num_data; i++) {
        if (min_log[i] < total_min) total_min = min_log[i];
        if (max_log[i] > total_max) total_max = max_log[i];
      }
    }
  }

  // グラフを描画する関数
  void render() {
    if (num_data <= 0) return;

    canvas.fillRect(left, top, WIDTH, HEIGHT, LCD_BLACK);

    float total_center = (total_min + total_max) / 2;  // 中間データ
    float y_range = total_max - total_min;  // 表示データの範囲
    if (y_range < min_range) y_range = min_range;
    float y_zoom = HEIGHT / y_range;
    int y_offset = top + HEIGHT / 2 + total_center * y_zoom;  // グラフ下端

    // Serial.printf("%f %f %f %d\n", total_center, y_range, y_zoom, y_offset);

    // グラフ右端から左端に向けて走査
    int x_plot = left + WIDTH - 1;
    for (int i = 0; i < num_data; i++) {
      // shift するまでに push されたデータの最大値から最小値まで縦線を引く
      int y_top    = y_offset - max_log[i] * y_zoom;
      int y_bottom = y_offset - min_log[i] * y_zoom;
      canvas.drawFastVLine(x_plot, y_top, y_bottom - y_top + 1, plot_color);
      // Serial.printf("x:%d y:%d h:%d top:%d bottom:%d\n", x_plot, y_top, y_bottom - y_top, y_top, y_bottom);
      canvas.drawFastVLine(x_plot, y_bottom+1, top + HEIGHT - (y_bottom+1) , fill_color);
      // canvas.drawFastVLine(x_plot, y_bottom+1, 2 , 0xF800);
      x_plot--;
    }

    // スケール垂直線を描画
    for (int x = left + WIDTH - 1; x > left; x -= VERTICAL_LINE_STEP) {
      drawInvertDottedVLine(x, top, HEIGHT, 1, 2);
    }

    // Y軸のスケール計算
    float level_top = (y_offset - top) / y_zoom;
    float level_bottom = (y_offset - (top + HEIGHT)) / y_zoom;
    float y_scale = level_top - level_bottom;

    // 水平線の間隔を決定
    if      (y_scale > 1000) horizontal_line_step = 500;
    else if (y_scale >  500) horizontal_line_step = 200;
    else if (y_scale >  200) horizontal_line_step = 100;
    else if (y_scale >  100) horizontal_line_step =  50;
    else if (y_scale >   50) horizontal_line_step =  20;
    else if (y_scale >   20) horizontal_line_step =  10;
    else if (y_scale >   10) horizontal_line_step =   5;
    else if (y_scale >    5) horizontal_line_step =   2;
    else if (y_scale >    2) horizontal_line_step =   1;
    else if (y_scale >    1) horizontal_line_step =   0.5f;
    else                     horizontal_line_step =   0.2f;
    
    // 水平線を描画
    float level = horizontal_line_step * ceilf(level_bottom / horizontal_line_step);
    while (level < level_top) {
      int y_line = y_offset - level * y_zoom;
      drawDottedHLine(left, y_line, WIDTH, LCD_WHITE, 1, 2);
      level += horizontal_line_step;
    }

  }

  // 現在の測定値表示
  void drawValue(float value, char * value_format, char * unit) {
    canvas.setTextColor(LCD_WHITE);
    const GFXfont * value_font = &FreeSans18pt7b;
    const GFXfont * unit_font = &FreeSans9pt7b;
    
    const int16_t cursor_x = left + 200;
    const int16_t cursor_y = top + 62;
    const int16_t offset_value_h = 3;
    const int16_t offset_value_v = 30;

    canvas.setCursor(cursor_x+offset_value_h, cursor_y-offset_value_v);
    canvas.setFont(value_font);
    canvas.printf(value_format, value);
    canvas.setFont(unit_font);
    canvas.print(unit);
  }
  
  // スケール、最小値/最大値
  void drawLegend(char * minmax_format, char * scale_format) {
    canvas.setTextColor(LCD_WHITE);
    const GFXfont * minmax_font = &FreeSans9pt7b;
    const GFXfont * scale_font = &Org_01;

    const int16_t cursor_x = left + 200;
    const int16_t cursor_y = top + 60;

    const int16_t offset_minmax_h = 30;
    const int16_t offset_minmax_v = 8;
    canvas.setCursor(cursor_x+offset_minmax_h, cursor_y-offset_minmax_v);
    canvas.setFont(minmax_font);
    canvas.printf(minmax_format, total_min, total_max);

    // スケール
    canvas.setCursor(cursor_x+10, cursor_y-12);
    canvas.setFont(scale_font);
    canvas.printf(scale_format, horizontal_line_step);

    // スケール図形
    const int16_t scale_arrow_size = 13;
    const int16_t scale_arrow_y1 = cursor_y-23+2;
    const int16_t scale_arrow_y2 = scale_arrow_y1 + scale_arrow_size;
    const int16_t scale_arrow_x = cursor_x+6;
    drawInvertDottedHLine(cursor_x+3, cursor_y-5, 8, 1, 2);
    drawInvertDottedHLine(cursor_x+3, cursor_y-23, 8, 1, 2);
    canvas.drawFastVLine(scale_arrow_x, scale_arrow_y1, scale_arrow_size, LCD_WHITE);
    canvas.drawLine(scale_arrow_x, scale_arrow_y1 , scale_arrow_x-2, scale_arrow_y1+2, LCD_WHITE);  // 左下へ
    canvas.drawLine(scale_arrow_x, scale_arrow_y1 , scale_arrow_x+2, scale_arrow_y1+2, LCD_WHITE);  // 右下へ
    canvas.drawLine(scale_arrow_x, scale_arrow_y2 , scale_arrow_x-2, scale_arrow_y2-2, LCD_WHITE);  // 左上へ
    canvas.drawLine(scale_arrow_x, scale_arrow_y2 , scale_arrow_x+2, scale_arrow_y2-2, LCD_WHITE);  // 右上へ
  }
};

// MH-Z19C
uint8_t ReadCO2[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
uint8_t SelfCalOn[9]  = {0xFF, 0x01, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6};
uint8_t SelfCalOff[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86};

void mhz19c_init() {
  uint8_t resp[9];

  pinMode(UART_RX, INPUT);
  pinMode(UART_TX, OUTPUT);

  Serial1.begin(9600);
  Serial1.write(SelfCalOff, sizeof SelfCalOff);
  delay(100);

  Serial1.readBytes((char *)resp, sizeof resp);
  delay(100);
}

bool mhz19c_measure(int *result) {
  uint8_t resp[9];

  Serial1.write(ReadCO2,sizeof ReadCO2);
  Serial1.readBytes((char *)resp, sizeof resp);
  
  if (resp[0] == 0xff && resp[1] == 0x86) {
      *result = resp[2] * 256 + resp[3];
      return true;
  }
  else {
      *result = -1;
      return false;
  }
}

void setup() {
  // SPI 初期設定
  SPI.setTX(SPI_MOSI);
  SPI.setRX(SPI_MISO);
  SPI.setSCK(SPI_SCK);
  SPI.begin();

  // 液晶画面初期設定
  lcd.begin();
  lcd.setRotation(LCD_ROTATION);

  // センサ
  bme280.begin(BME280_CS, &SPI);
  bme280.setSettings(Bme280Settings::indoor());
  mhz19c_init();

  Serial.begin(9600);
  Serial.printf("Hello RP2040-env-mon!\n");
}

void loop() {
  // グラフのシフトタイミング
  static unsigned long next_lcd_shift_ms = GRAPH_SHIFT_INTERVAL_MS;
  bool shift = false;
  if (millis() >= next_lcd_shift_ms) {
    // millis() + GRAPH_SHIFT_INTERVAL_MS がオーバーフローしたとき、
    // 2連続で shift してしまうが、頻度は少ないので無視する
    next_lcd_shift_ms = millis() + GRAPH_SHIFT_INTERVAL_MS;
    shift = true;
    Serial.println("lcd shift!");
  }

  // グラフ
  canvas.fillScreen(LCD_BLACK);
  static Graph graph_t(0,   0,  1.0f, LCD_WHITE, get16bitColor(1.0f, 0.5f, 0.5f));  // temperature: 気温
  static Graph graph_h(0,  60, 10.0f, LCD_WHITE, get16bitColor(0.5f, 0.5f, 1.0f));  // humidity: 湿度
  static Graph graph_p(0, 120,  1.0f, LCD_WHITE, get16bitColor(0.5f, 1.0f, 0.5f));  // pressure: 気圧
  static Graph graph_c(0, 180, 10.0f, LCD_WHITE, get16bitColor(0.5f, 0.5f, 0.5f));  // CO2
  
  // BME280
  static float pre_temperature = 20.0;
  float t = bme280.getTemperature() + TEMPERATURE_OFFSET;
  float temperature;
  if (isfinite(t)) {
    temperature = t;
    pre_temperature = t;
  } else {
    // 無限大取得時の対応（稀に検出）
    temperature = pre_temperature;
  }
  float pressure = bme280.getPressure();
  float humidity = bme280.getHumidity();
  
  // MH-Z19C
  int co2 = 0;
  mhz19c_measure(&co2);
  
  // デバッグ用
  Serial.printf("%.1f ℃, %.1f %%, %.f hPa, %d ppm\n", temperature, humidity, pressure/100, co2);
  
  // グラフ描画
  graph_t.push(temperature, shift);
  graph_h.push(humidity, shift);
  graph_p.push(pressure/100, shift);
  graph_c.push(co2, shift);
  graph_t.render();
  graph_h.render();
  graph_p.render();
  graph_c.render();

  // 水平線
  canvas.drawFastHLine(0,  60-1, LCD_WIDTH, LCD_WHITE);
  canvas.drawFastHLine(0, 120-1, LCD_WIDTH, LCD_WHITE);
  canvas.drawFastHLine(0, 180-1, LCD_WIDTH, LCD_WHITE);

  // 値 + 単位
  graph_t.drawValue(temperature, "%.1f", " C");
  graph_h.drawValue(humidity, "%.1f", " %");
  graph_p.drawValue(pressure/100, "%.f", " hPa");
  graph_c.drawValue(co2, "%.f", " ppm");

  // 最小値/最大値 + 1div
  graph_t.drawLegend("%.1f/%.1f", "%.1f");
  graph_h.drawLegend("%.1f/%.1f", "%.1f");
  graph_p.drawLegend("%.f/%.f", "%.1f");
  graph_c.drawLegend("%.f/%.f", "%.f");

  // TFTに描画
  lcd.drawRGBBitmap(0, 0, canvas.getBuffer(), LCD_WIDTH, LCD_HEIGHT);

  // デバッグ用
  // Serial.printf("millis(): %lu\n", millis());

  delay(SAMPLING_INTERVAL_MS);
}
