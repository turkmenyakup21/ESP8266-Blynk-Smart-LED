// =====================================================================================================
// 1. BLYNK VE BULUT BAĞLANTI TANIMLAMALARI (Kullanıcı kendi bilgilerini girmelidir)
// =====================================================================================================
#define BLYNK_TEMPLATE_ID   "BLYNK_TEMPLATE_ID_YAZINIZ"
#define BLYNK_TEMPLATE_NAME "şerit led kontrol"
#define BLYNK_AUTH_TOKEN    "BLYNK_AUTH_TOKEN_YAZINIZ"

// =====================================================================================================
// 2. GEREKLİ KÜTÜPHANELERİN TANIMLANMASI 
// =====================================================================================================

#include <ESP8266WiFi.h>          // ESP8266'nın dahili Wi-Fi modülünü kontrol eder; ağları taramayı ve modeme bağlanmayı sağlar.
#include <BlynkSimpleEsp8266.h>   // Telefon uygulaması ile kart arasındaki internet/bulut iletişim köprüsünü kurar.
#include <EEPROM.h>               // Elektrik kesildiğinde LED'lerin son durumunun ve parlaklığının silinmemesi için kalıcı hafızayı yönetir.
#include <WiFiManager.h>          // İlk açılış anında wifiye bağlanmak için gerekli kütüphane.

// =====================================================================================================
// 3. DONANIM (PİN) VE SİSTEM HAFIZA DEĞİŞKENLERİ
// =====================================================================================================
// Cihazın üzerindeki fiziksel pin tanımlamaları 
const int ledPins[3] = {5, 4, 14};    // MOSFET sürücülerin bağlı olduğu PWM pinleri:  D1, D2, D5
const int butonPins[3] = {0, 12, 13}; // Fiziksel anahtarların bağlı olduğu pinler: D3, D6, D7

// [FABRİKA AYARLARINA DÖNÜŞ BUTONU]: Hafızadaki Wi-Fi şifresini silmek için kullanacağımız güvenli donanım pini.
const int resetButonPin = 16; // NodeMCU üzerindeki D0 pini

// Sistem çalışırken anlık durumları RAM üzerinde tutan çalışma değişkenleri
// (Not: Elektrik kesintisine karşı bu veriler kodun devamında EEPROM kalıcı belleğine yedeklenir)
int ledHafizaParlaklik[3] = {100, 100, 100}; // Kullanıcının en son ayarladığı parlaklık yüzdesi (%0 - %100 arası)
bool ledAcikMi[3] = {false, false, false};   // LED'lerin anlık açık/kapalı bilgisi (true = Açık, false = Kapalı)

// Arapuar anahtarlar kalıcı tipte olduğu için mekanik konum değişimlerini (durum değişikliği) yakalamak için son durumları tutulur
int sonButonDurumlari[3] = {HIGH, HIGH, HIGH};

// =====================================================================================================
// 4. EEPROM (KALICI BELLEK) ADRES HARİTASI
// =====================================================================================================
// ESP8266'nın kalıcı flaş belleğinde verilerin yazılacağı hücre numaraları.
// Her kanalın verisi (parlaklık ve durum) birbirinin üzerine yazılmasın diye benzersiz adreslere atanır.
const int EEPROM_PARLAKLIK_ADRES[3] = {0, 2, 4}; // 0, 2 ve 4. hücreler parlaklık yüzdesi için
const int EEPROM_DURUM_ADRES[3] = {1, 3, 5};      // 1, 3 ve 5. hücreler açık/kapalı (0 veya 1) durumu için

// =====================================================================================================
// 5. ÇEKİRDEK FONKSİYONLAR (SİSTEM MOTORU)
// =====================================================================================================

// --- FONKSİYON: LED'LERİ FİZİKSEL OLARAK SÜREN KISIM ---
// Bu fonksiyon çağrıldığında parametre olarak aldığı indekse (0,1,2) göre ilgili pindeki PWM sinyalini günceller.
void ledDonanimSur(int index) {
  int pwmDegeri;

  // Eğer sistem hafızasında LED açık olarak işaretlendiyse parlaklık hesabını yap
  if (ledAcikMi[index]) {
    // [MÜHENDİSLİK HARİTALAMA]: Blynk'ten gelen %0-%100 arası değeri, transistörün sönme sınırı olan 263 ile tam güç olan 0 arasına eşliyoruz.
    // Transistör sinyali evirdiği (Invert ettiği) için: %100 parlaklık = 0 PWM sinyali, %0 parlaklık = 263 PWM sinyali demektir.
    pwmDegeri = map(ledHafizaParlaklik[index], 0, 100, 263, 0);
  } else {
    // Eğer LED kapalıysa, donanımın akımı tamamen kesmesi için pinden tam 1023 (HIGH) kesim sinyali gönderilir.
    pwmDegeri = 1023; 
  }
  
  // Hesaplanan analog PWM değerini (0-1023 arası) ilgili NodeMCU fiziksel çıkış pinine yazar.
  analogWrite(ledPins[index], pwmDegeri);

  // Geri bildirim (Feedback): Cihaz üzerinden durum değiştiğinde, telefondaki Blynk arayüzünün de güncellenmesini sağlar.
  Blynk.virtualWrite(V0 + index, ledAcikMi[index] ? 1 : 0);      // Sanal buton durumunu eşitle (V0, V1, V2)
  Blynk.virtualWrite(V3 + index, ledHafizaParlaklik[index]);    // Sanal slider konumunu eşitle (V3, V4, V5)
}

// --- FONKSİYON: EEPROM'A KAYIT YAPMA ---
// Elektrik kesintilerine karşı verileri kalıcı flaş belleğe işleyen fonksiyon.
void eepromKaydet(int index) {
  EEPROM.write(EEPROM_PARLAKLIK_ADRES[index], ledHafizaParlaklik[index]); // Parlaklık yüzdesini yaz
  EEPROM.write(EEPROM_DURUM_ADRES[index], ledAcikMi[index] ? 1 : 0);      // Açık/kapalı durumunu yaz (1 veya 0)
  EEPROM.commit(); // [KRİTİK]: Yazılan verileri RAM'den alıp flaş belleğe fiziksel olarak mühürler/kaydeder.
}

// Telefondaki uygulamadan anlık butonlara (V0, V1, V2) dokunulduğunda çalışan fonksiyon
void blynkButonHareketi(int index, int durum) {
  ledAcikMi[index] = (durum == 1); // Gelen durum 1 ise true (Açık), 0 ise false (Kapalı) yap
  ledDonanimSur(index);            // Değişikliği donanıma uygula
  eepromKaydet(index);            // Değişikliği kalıcı belleğe yaz
}

// Telefondaki uygulamadan anlık parlaklık sliderları (V3, V4, V5) kaydırıldığında çalışan fonksiyon
void blynkSliderHareketi(int index, int parlaklik) {
  if (parlaklik > 0) {
    ledHafizaParlaklik[index] = parlaklik; // Gelen 0-100 arası değeri hafızaya al
    ledAcikMi[index] = true;               // Eğer parlaklık sıfırdan büyükse lambayı otomatik açık yap
  } else {
    ledAcikMi[index] = false;              // Slider en sola (%0) çekildiyse lambayı kapat
  }
  ledDonanimSur(index); // Değişiklikleri şerit LED'e yansıt
  eepromKaydet(index);  // Değişiklikleri kalıcı hafızaya yaz
}

// Fiziksel çıt çıt anahtara tıklandığında (Durumu tersine çevirme)
void ledToggle(int index) {
  ledAcikMi[index] = !ledAcikMi[index]; // Ünlem işareti mevcut bool değeri tersine çevirir (Açıksa kapatır, kapalıysa açar)
  ledDonanimSur(index);                  // Yeni durumu donanıma uygula
  eepromKaydet(index);                  // Yeni durumu kalıcı belleğe kaydet
}

// =====================================================================================================
// 6. BLYNK SANAL PİN TETİKLENMELERİ (BLYNK_WRITE)
// =====================================================================================================
// Telefondan bir komut geldiğinde Blynk sunucusu bu fonksiyonları anlık olarak tetikler.
BLYNK_WRITE(V0) { blynkButonHareketi(0, param.asInt()); } // 1. Kanal Butonu
BLYNK_WRITE(V1) { blynkButonHareketi(1, param.asInt()); } // 2. Kanal Butonu
BLYNK_WRITE(V2) { blynkButonHareketi(2, param.asInt()); } // 3. Kanal Butonu

BLYNK_WRITE(V3) { blynkSliderHareketi(0, param.asInt()); } // 1. Kanal Slider
BLYNK_WRITE(V4) { blynkSliderHareketi(1, param.asInt()); } // 2. Kanal Slider
BLYNK_WRITE(V5) { blynkSliderHareketi(2, param.asInt()); } // 3. Kanal Slider

// =====================================================================================================
// 7. SETUP (BAŞLANGIÇ AYARLARI)
// =====================================================================================================
// Kart ilk enerji aldığında sadece bir kereye mahsus çalışan, donanımı hazırlayan bölüm.
void setup() {
  // EEPROM kalıcı bellek alanını kullanabilmek için 512 byte'lık sanal bir sektör açıyoruz.
  EEPROM.begin(512);

  // Sıfırlama (Reset) butonunun bağlı olduğu D0 pinini giriş olarak ayarlıyoruz ve dahili Pull-up direncini açıyoruz.
  pinMode(resetButonPin, INPUT_PULLUP);

  // 3 kanal için de donanım pinlerini ve açılış hafıza kontrollerini döngüyle hazırlıyoruz
  for(int i = 0; i < 3; i++) {
    pinMode(ledPins[i], OUTPUT);         // LED pinlerini donanımsal çıkış olarak tanımla
    pinMode(butonPins[i], INPUT_PULLUP); // Çıt çıt buton pinlerini gürültüyü önlemek için dahili pull-up ile giriş tanımla
    sonButonDurumlari[i] = digitalRead(butonPins[i]); // Açılışta anahtarın o anki fiziksel konumunu hafızaya al
    
    // --- KALICI BELLEKTEN ESKİ VERİLERİ OKUMA ---
    int okunanParlaklik = EEPROM.read(EEPROM_PARLAKLIK_ADRES[i]); // Son parlaklığı oku
    int okunanDurum = EEPROM.read(EEPROM_DURUM_ADRES[i]);           // Son On/Off durumunu oku

    // [GÜVENLİK FİLTRESİ]: Eğer hafıza ilk defa okunuyorsa içi boş (255) gelebilir. 
    // Veriler saçma geldiyse sistem kilitlenmesin diye varsayılan güvenli açılış değerlerini yüklüyoruz.
    if (okunanParlaklik < 0 || okunanParlaklik > 100) okunanParlaklik = 100;
    if (okunanDurum < 0 || okunanDurum > 1) okunanDurum = 0;

    // Temizlenen kararlı verileri sistem çalışma RAM değişkenlerine aktar
    ledHafizaParlaklik[i] = okunanParlaklik;
    ledAcikMi[i] = (okunanDurum == 1);

    // Elektrik kesilip geldiğinde lambaları hafızadaki son konumda aynen geri yak/söndür.
    ledDonanimSur(i);
  }

  // --- WIFIMANAGER AKILLI BAĞLANTI BAŞLANGICI ---
  WiFiManager wm; 
  // Hafızada kayıtlı eski ağ yoksa otomatik olarak "Akilli_Led_Kurulum" adında şifresiz bir hotspot açar.
  // Kullanıcı telefondan bağlanıp ev şifresini girip kaydedene kadar kod bu satırda bekler.
  bool baglantiDurumu = wm.autoConnect("Akilli_Led_Kurulum");

  if(!baglantiDurumu) {
    ESP.restart(); // Eğer kurulum esnasında bir hata oluşursa kartı güvenle yeniden başlat
  }

  // Wi-Fi bağlantısı başarıyla sağlandıktan sonra Blynk bulut protokolünü sadece Token ile başlatıyoruz.
  Blynk.config(BLYNK_AUTH_TOKEN);
}

// =====================================================================================================
// 8. LOOP (SÜREKLİ DÖNGÜ)
// =====================================================================================================
// Kart açık olduğu sürece saniyede binlerce kez çalışan, asla durmayan ana döngü.
void loop() {
  Blynk.run(); // Blynk bulut sunucusu ile veri alışverişini sürekli canlı ve senkron tutar.
  
  // =====================================================================================================
  // [HAFIZA SIFIRLAMA BUTONU KONTROLÜ]
  // =====================================================================================================
  // Butona fiziksel olarak basıldığında GND'ye temas edeceği için pinden LOW (0) sinyali okunur.
  if (digitalRead(resetButonPin) == LOW) {
    unsigned long basmaBaslangicZamani = millis(); // Butona ilk basıldığı anki dahili zamanı milisaniye olarak kaydet
    
    // Kullanıcı elini butondan çekmediği sürece ve henüz 5 saniye dolmadıysa bu döngünün içinde bekle
    while (digitalRead(resetButonPin) == LOW) {
      // Eğer butona kesintisiz basma süresi 5 saniyeyi (5000 ms) geçtiyse sıfırlamayı başlat
      if (millis() - basmaBaslangicZamani >= 5000) {
        
        // [GÖRSEL UYARI]: Sıfırlamanın başladığını kullanıcıya bildirmek için tüm LED'leri 3 kez flaş ettir
        for (int k = 0; k < 3; k++) {
          analogWrite(ledPins[0], 0); analogWrite(ledPins[1], 0); analogWrite(ledPins[2], 0); // Hepsini tam güç yak
          delay(200);
          analogWrite(ledPins[0], 1023); analogWrite(ledPins[1], 1023); analogWrite(ledPins[2], 1023); // Hepsini tamamen söndür
          delay(200);
        }

        // WiFiManager'ın flash belleğe kaydettiği tüm Wi-Fi adlarını ve şifrelerini tamamen sil!
        WiFiManager wm;
        wm.resetSettings(); 
        
        // Kartı resetle. Cihaz hafızası boş olarak yeniden açılacağı için direkt kurulum moduna geçecektir.
        ESP.restart(); 
      }
      delay(10); // İşlemciyi gereksiz yormamak ve kilitlememek için küçük bir nefes alma payı
    }
  }

  // =====================================================================================================
  // [FİZİKSEL ARAPUAR ANAHTARLARININ TARANMASI]
  // =====================================================================================================
  for(int i = 0; i < 3; i++) {
    int mevcutButonDurumu = digitalRead(butonPins[i]); // Düğmenin o anki fiziksel lojik durumunu anlık oku
    
    // Eğer düğme eski konumundan farklı bir konuma geçtiyse (yani kullanıcı düğmeyi çıt yaptıysa)
    if (mevcutButonDurumu != sonButonDurumlari[i]) {
      delay(50); // [DEBOUNCE FILTER]: Metal kontakların ilk temas anındaki elektriksel ark/gürültü sıçramalarını önlemek için 50ms bekle.
      
      // Bekleme süresi sonunda sinyal hala kararlıysa (gürültü değilse, gerçekten anahtara basıldıysa)
      if (digitalRead(butonPins[i]) == mevcutButonDurumu) { 
        ledToggle(i); // Lambanın açık/kapalı durumunu altüst et (Toggle fonksiyonuna git)
        sonButonDurumlari[i] = mevcutButonDurumu; // Yeni fiziksel konumu hafızaya sabitle ki bir sonraki çıt hareketine kadar beklesin.
      }
    }
  }
}