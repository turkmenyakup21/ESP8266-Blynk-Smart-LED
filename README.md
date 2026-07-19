# Blynk ve ESP8266 ile Akıllı Şerit LED Kontrolü

Bu proje, NodeMCU (ESP8266) kullanarak 3 kanallı şerit LED dimmer sistemini hem fiziksel anahtarlar hem de Blynk bulut servisi üzerinden kontrol etmeyi sağlayan akıllı bir ev otomasyonu donanım ve yazılım projesidir.

## ✨ Öne Çıkan Özellikler
- **WiFiManager Entegrasyonu:** Wi-Fi kimlik bilgileri koda gömülmez, web arayüzü üzerinden dinamik olarak kurulur.
- **EEPROM Hafıza:** Elektrik kesintilerinde son parlaklık seviyeleri ve LED durum verileri güvenle saklanır, sistem kaldığı yerden devam eder.
- **Fabrika Ayarları (Reset) Butonu:** D0 pinine bağlı butona 5 saniye basılı tutulduğunda cihazın Wi-Fi hafızası temizlenir ve yeniden kuruluma hazır hale gelir.

## 🔌 Donanım ve Devre Mimarisi
Projenin devre şeması Altium Designer üzerinde olarak çizilmiştir. 
- Yüksek çözünürlüklü ve vektörel şema incelemesi için proje klasöründeki **PDF şema dosyasını** bilgisayarınıza indirebilirsiniz.
- Lojik anahtarlamayı güvenli kılmak adına ESP çıkışlarında transistörlü MOSFET sürücü katı kullanılmıştır.

## ⚠️ Önemli Not
Kodu kendi kartınıza yüklemeden önce Blynk uygulamasından aldığınız `Template ID` ve `Auth Token` bilgilerini ilgili alanlara yazmayı unutmayınız. Güvenliğiniz için bu alanlar şablonda boş bırakılmıştır.
