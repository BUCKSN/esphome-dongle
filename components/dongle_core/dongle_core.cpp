#include "dongle_core.h"

#include <cstdio>
#include <cstring>

#include "esphome/core/alloc_helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif
#ifdef USE_API
#include "esphome/components/api/api_server.h"
#endif
#ifdef USE_WIFI
#include <esp_wifi.h>
#include "esphome/components/wifi/scan_list.h"
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "esphome/components/network/util.h"

namespace esphome::dongle_core {

static const char *const TAG = "dongle_core";
static const uint32_t LEDS_PREF_HASH = 0x444E4C32;
static const uint8_t LEDS_OFF = 2;  // любое другое значение, в том числе пустая память: светодиоды включены
static const uint32_t SCAN_INTERVAL_MS = 30000;
static const uint32_t FILTER_PREF_HASH = 0x444E4654;
static const uint32_t FILTER_SAVE_EVERY_MINUTES = 60;
static const uint32_t ROLLBACK_PREF_HASH = 0x444E4757;
static const uint32_t ROLLBACK_TIMEOUT_MS = 60000;
static const uint32_t APP_DESC_MAGIC = 0xABCD5432;
static const size_t APP_DESC_OFFSET = 32;  // заголовок образа 24 байта и заголовок сегмента 8 байт
static const size_t IMAGE_HEADER_MIN = APP_DESC_OFFSET + 4;

static const char PAGE[] = R"HTML(<!DOCTYPE html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>esphome-dongle</title>
<style>
body{font:18px system-ui;margin:0;padding:16px;background:#111;color:#eee;max-width:700px}
button{font-size:20px;padding:16px;border:0;border-radius:8px;margin:6px 0;width:100%;color:#fff;background:#444}
button:disabled{opacity:.5}#lang{width:auto;padding:8px 14px;font-size:16px;float:right}#learn{background:#2980b9}
h2{margin:26px 0 4px;font-size:22px}p{color:#bbb;margin:6px 0}a{color:#6cb6ff}#warn{color:#f39c12}
table{width:100%;border-collapse:collapse}td{padding:5px 0;border-bottom:1px solid #333}td:first-child{color:#999;width:45%}
label{display:block;margin:10px 0 4px}input[type=text],input[type=password]{font:18px system-ui;padding:12px;width:100%;box-sizing:border-box;border-radius:8px;border:0}
.net{display:flex;justify-content:space-between;font-size:18px;padding:12px;margin:4px 0;text-align:left;background:#222}.net.on{background:#2980b9}
#wifi{background:#1a1a1a;border-radius:8px;padding:0 12px 12px;margin-top:14px}
#key{font:15px monospace;word-break:break-all;background:#222;padding:10px;border-radius:6px;display:none}
</style>
<button id="lang"></button><h1 id="h"></h1><p id="about"></p>
<table id="tbl"></table><p id="left"></p>
<div id="wifi" style="display:none"><h2 id="wh"></h2><p id="wp"></p><div id="nets"></div>
<label id="wsl" for="wssid"></label><input type="text" id="wssid" autocomplete="off">
<label id="wpl" for="wpsk"></label><input type="password" id="wpsk" autocomplete="off">
<label><input type="checkbox" id="wshow" style="width:auto"> <span id="wsh"></span></label>
<button id="wsave"></button><p id="wmsg"></p></div>
<h2 id="lh"></h2><p id="lp"></p><button id="leds" onclick="post('leds')"></button>
<h2 id="kh"></h2><p id="kp"></p><div id="key"></div><button id="kcopy" style="display:none;background:#27ae60"></button><button id="kshow"></button><button id="knew"></button>
<h2 id="ffh"></h2><p id="ffp"></p><p id="ffv"></p><button id="ffr" onclick="post('filter/reset')"></button>
<h2 id="fwh"></h2><p id="fwp"></p><p id="fwcur"></p><button id="upd"></button><p id="updm"></p><input type="file" id="file" accept=".bin"><p id="fwmsg"></p><p id="warn"></p>
<button id="fwgo" disabled></button>
<h2 id="dh"></h2><p id="dp"></p><button id="learn" onclick="location.href='/dongle/learn'"></button>
<h2 id="ch"></h2><p id="cp"></p><button id="close" onclick="post('close')"></button>
<p id="links"></p>
<script>
const $=id=>document.getElementById(id);
const T={ru:{ffh:'Фильтр',ffp:'Считает часы работы прибора и напоминает о чистке фильтра. После чистки сбросьте счётчик.',ffv:'Наработка: %a ч из %b ч',ffn:' Пора чистить фильтр.',ffr:'Фильтр почищен, сбросить счётчик',upd:'Проверить обновления',updl:'Новая версия: ',updn:'Установлена последняя версия',upde:'Не удалось проверить, нужен интернет на этом устройстве',wh:'Подключение к Wi-Fi',wp:'Донгл пока не подключён к сети. Выберите свою сеть и введите пароль. После сохранения эта точка доступа выключится, а телефон вернётся в обычную сеть. Затем коротко нажмите кнопку на донгле и откройте страницу по адресу из роутера или esphome-dongle-XXXXXX.local.',wsl:'Сеть',wpl:'Пароль',wsh:'Показать пароль',wsave:'Подключиться',wh2:'Сменить сеть Wi-Fi',wp2:'Донгл перезагрузится и подключится к новой сети. Если за минуту не получится, например из-за неверного пароля, он сам вернётся на текущую сеть. Новый адрес смотрите в роутере или используйте esphome-dongle-XXXXXX.local.',wsaved2:'Сохранено, донгл перезагружается. Если новая сеть не подключится, через минуту он вернётся на прежнюю.',ssid:'Сеть Wi-Fi',wsaved:'Сохранено, донгл подключается. Если не получится, например из-за неверного пароля, сеть донгла останется доступной: подключитесь к ней снова и проверьте пароль.',wfail:'Не удалось сохранить, попробуйте ещё раз.',wscan:'Ищу сети…',wneed:'Введите название сети',aleft:'Открыто, пока Wi-Fi не настроен',mac:'MAC-адрес',aabout:'Режим настройки: донгл создал свою открытую сеть, поэтому к нему можно подключиться с телефона или компьютера. Интернет здесь недоступен. Эта страница работает, пока Wi-Fi не настроен, и пользоваться ей может любой, кто рядом.',h:'Донгл ESPHome',about:'Страница настройки донгла. Она открыта временно: кнопкой на донгле или переключателем в Home Assistant. Пока она открыта, ею может пользоваться любое устройство в этой сети.',
name:'Имя в сети',ip:'IP-адрес',rssi:'Сигнал Wi-Fi',up:'Время работы',link:'Связь с устройством',yes:'есть',no:'нет',ver:'Прошивка',board:'Плата',dev:'Устройство',
left:'Закроется без действий через %d мин',dh:'Добавить своё устройство в прошивку',dp:'Для тех, кто хочет собрать данные о своём приборе и отправить их разработчику, чтобы в прошивке появилась его поддержка. Это не проверка исправности донгла. Записывает, что меняет каждая кнопка пульта или панели, и собирает отчёт с фото. Занимает 15-30 минут, нужен телефон и пульт. Устройству не отправляет команд, данные остаются на телефоне, пока вы сами их не отправите.',learn:'Добавить своё устройство',
lh:'Светодиоды',lp:'Реакция на короткое нажатие кнопки: синий 3 с - страница открыта, зелёный - закрыта и Wi-Fi есть, красный - закрыта и Wi-Fi нет. Удержание кнопки: через 2 с горит красный, через 10 с он мигает и настройки сбрасываются.',leds:'Светодиоды кнопки: ',on:'включены',off:'выключены',
kh:'Ключ Home Assistant',kp:'Шифрует связь с Home Assistant. Донгл создал его сам при первом запуске. Введите его, когда Home Assistant запросит ключ при добавлении устройства. Сброс кнопкой на 10 с стирает ключ.',kshow:'Показать ключ',khide:'Скрыть ключ',kcopy:'Скопировать ключ',kdone:'Скопировано',kfail:'Не удалось: выделите ключ вручную',kno:'Ключа нет',knew:'Создать новый ключ',kask:'Home Assistant потеряет связь, пока вы не введёте новый ключ. Продолжить?',
fwh:'Обновление прошивки',fwp:'Нужен файл .ota.bin из релиза проекта. Файл проверяется до установки, при ошибке текущая прошивка остаётся.',fwcur:'Установлена: ',fwgo:'Установить',chk:'Совместима, версия ',older:'Этот файл старше установленной прошивки.',wrong:'Не подходит: ',eimg:'это не образ приложения (factory.bin не подходит, нужен ota.bin)',echip:'файл для другого чипа',emark:'это не прошивка этого проекта',eboard:'файл для другого донгла или устройства',send:'Установка: ',ok:'Установлено, идёт перезапуск. Страница вернётся примерно через минуту.',fail:'Ошибка обновления: ',lost:'Связь потеряна',
ch:'Закрыть веб-интерфейс',cp:'Веб-интерфейс закрывается сам через час без действий. Открыть снова можно кнопкой на донгле.',close:'Закрыть веб-интерфейс',closed:'Веб-интерфейс закрыт. Откройте её кнопкой на донгле или в Home Assistant.',docs:'Документация и список устройств'},
en:{ffh:'Filter',ffp:'Counts appliance running hours and reminds you to clean the filter. Reset the counter after cleaning.',ffv:'Running time: %a h of %b h',ffn:' Time to clean the filter.',ffr:'Filter cleaned, reset the counter',upd:'Check for updates',updl:'New version: ',updn:'You have the latest version',upde:'Could not check, this device needs internet',wh:'Connect to Wi-Fi',wp:'The dongle is not on a network yet. Choose your network and enter the password. After saving, this access point turns off and your phone returns to its usual network. Then short-press the dongle button and open the page by the address from your router or esphome-dongle-XXXXXX.local.',wsl:'Network',wpl:'Password',wsh:'Show password',wsave:'Connect',wh2:'Change Wi-Fi network',wp2:'The dongle restarts and joins the new network. If that fails within a minute, for example because of a wrong password, it returns to the current network by itself. Find the new address in your router or use esphome-dongle-XXXXXX.local.',wsaved2:'Saved, the dongle is restarting. If the new network fails, it returns to the previous one in a minute.',ssid:'Wi-Fi network',wsaved:'Saved, the dongle is connecting. If this fails, for example because of a wrong password, the dongle network stays available: join it again and check the password.',wfail:'Could not save, try again.',wscan:'Searching for networks…',wneed:'Enter a network name',aleft:'Open until Wi-Fi is set up',mac:'MAC address',aabout:'Setup mode: the dongle created its own open network so a phone or computer can reach it. There is no internet here. This page works until Wi-Fi is set up, and anyone nearby can use it.',h:'ESPHome dongle',about:'Dongle settings page. It opens temporarily with the dongle button or a Home Assistant switch. While it is open, any device on this network can use it.',
name:'Network name',ip:'IP address',rssi:'Wi-Fi signal',up:'Uptime',link:'Device link',yes:'yes',no:'no',ver:'Firmware',board:'Board',dev:'Device',
left:'Closes after %d min of inactivity',dh:'Add your device to the firmware',dp:'For those who want to collect data about their appliance and send it to the developer so the firmware can support it. This is not a health check of your dongle. It records what each remote or panel button changes and builds a report with photos. Takes 15-30 minutes, you need a phone and the remote. It sends no commands to the device and data stays on the phone until you send it.',learn:'Add your device',
lh:'LEDs',lp:'Reaction to a short button press: blue for 3 s - page open, green - closed with Wi-Fi, red - closed without Wi-Fi. Holding the button: red after 2 s, at 10 s it blinks and settings are reset.',leds:'Button LEDs: ',on:'on',off:'off',
kh:'Home Assistant key',kp:'Encrypts the link to Home Assistant. The dongle created it on first start. Enter it when Home Assistant asks for the key while adding the device. A 10 s button reset erases the key.',kshow:'Show key',khide:'Hide key',kcopy:'Copy key',kdone:'Copied',kfail:'Failed: select the key manually',kno:'No key',knew:'Create new key',kask:'Home Assistant loses connection until you enter the new key. Continue?',
fwh:'Firmware update',fwp:'Use the .ota.bin file from a project release. The file is checked before installing; on failure the current firmware stays.',fwcur:'Installed: ',fwgo:'Install',chk:'Compatible, version ',older:'This file is older than the installed firmware.',wrong:'Not compatible: ',eimg:'this is not an application image (factory.bin does not fit, use ota.bin)',echip:'file is for another chip',emark:'this is not firmware for this project',eboard:'file is for another dongle or device',send:'Installing: ',ok:'Installed, restarting. The page will return in about a minute.',fail:'Update failed: ',lost:'Connection lost',
ch:'Close web interface',cp:'The web interface closes itself after an hour of inactivity. Reopen it with the dongle button.',close:'Close web interface',closed:'The web interface is closed. Open it with the dongle button or in Home Assistant.',docs:'Documentation and device list'}};
let L=localStorage.getItem('dongle-lang')||((navigator.language||'').startsWith('ru')?'ru':'en');
const t=k=>T[L][k];
let info=null,fw=null,fwv='',saved=false,nets=[];
function texts(){document.documentElement.lang=L;for(const k of ['ffh','ffp','ffr','upd','wh','wp','wsl','wpl','wsh','wsave','h','about','dh','dp','learn','lh','lp','kh','kp','kshow','knew','fwh','fwp','fwgo','ch','cp','close'])$(k).textContent=t(k);$('lang').textContent=L=='ru'?'EN':'RU'}
$('lang').onclick=()=>{L=L=='ru'?'en':'ru';localStorage.setItem('dongle-lang',L);texts();tick()};
async function post(a){await fetch('/dongle/'+a,{method:'POST'});tick()}
function up(s){const d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;return(d?d+' d ':'')+h+':'+String(m).padStart(2,'0')}
async function tick(){try{const r=await fetch('/dongle/state');if(!r.ok)throw 0;info=await r.json();const s=info;
$('wifi').style.display=saved?'none':'';$('wh').textContent=s.ap?t('wh'):t('wh2');$('wp').textContent=s.ap?t('wp'):t('wp2');$('about').textContent=s.ap?t('aabout'):t('about');$('left').textContent=s.ap?t('aleft'):t('left').replace('%d',Math.ceil(s.left/60));$('leds').textContent=t('leds')+(s.leds?t('on'):t('off'));$('fwcur').textContent=t('fwcur')+s.version;$('ffv').textContent=t('ffv').replace('%a',s.filter).replace('%b',s.flimit)+(s.filter>=s.flimit?t('ffn'):'');
const b=$('tbl');b.replaceChildren();for(const [k,v] of [['name',s.name],['mac',s.mac],['ip',s.ip]].concat(s.ap?[]:[['ssid',s.ssid],['rssi',s.rssi+' dBm']],[['up',up(s.up)],['link',s.link?t('yes'):t('no')],['ver',s.version],['board',s.board],['dev',s.device]])){const tr=document.createElement('tr'),a=document.createElement('td'),c=document.createElement('td');a.textContent=t(k);c.textContent=v;tr.append(a,c);b.append(tr)}
const l=$('links');l.replaceChildren();if(s.project){const a=document.createElement('a');a.href=s.project;a.textContent=t('docs');l.append(a)}
}catch(e){if(saved)return;document.body.textContent=t('closed');clearInterval(timer)}}
async function showKey(){const k=$('key'),c=$('kcopy'),j=await(await fetch('/dongle/key')).json();k.textContent=j.key||t('kno');k.style.display='block';c.textContent=t('kcopy');c.style.display=j.key?'':'none';$('kshow').textContent=t('khide');keyOpen=true;post('touch')}
function hideKey(){$('key').style.display='none';$('kcopy').style.display='none';$('kshow').textContent=t('kshow');keyOpen=false}
// Страница работает по HTTP, где обычный буфер обмена браузера недоступен, поэтому есть запасной способ.
async function copyText(text){if(navigator.clipboard&&window.isSecureContext){try{await navigator.clipboard.writeText(text);return true}catch(e){}}
  const a=document.createElement('textarea');a.value=text;a.style.position='fixed';a.style.opacity='0';document.body.append(a);a.focus();a.select();a.setSelectionRange(0,text.length);let ok=false;try{ok=document.execCommand('copy')}catch(e){}a.remove();return ok}
let keyOpen=false;$('kshow').onclick=()=>keyOpen?hideKey():showKey();
$('kcopy').onclick=async()=>{const ok=await copyText($('key').textContent),c=$('kcopy');c.textContent=ok?t('kdone'):t('kfail');setTimeout(()=>{c.textContent=t('kcopy')},2000)};
$('knew').onclick=async()=>{if(!confirm(t('kask')))return;await post('key/new');setTimeout(showKey,1500)};
/*FWCHECK*/
function checkFirmware(b,info){
  if(b[0]!==0xE9||b.length<36||b[32]!==0x32||b[33]!==0x54||b[34]!==0xCD||b[35]!==0xAB)return{err:'eimg'};
  if((b[12]|(b[13]<<8))!==info.chip)return{err:'echip'};
  const m=/DONGLEFW1\|board=([A-Za-z0-9._-]+)\|device=([A-Za-z0-9._-]+)\|version=([A-Za-z0-9._-]+)\|/.exec(new TextDecoder('latin1').decode(b));
  if(!m)return{err:'emark'};
  if(m[1]!==info.board||m[2]!==info.device)return{err:'eboard'};
  return{version:m[3]};
}
/*FWCHECKEND*/
$('upd').onclick=async()=>{const m=$('updm');m.replaceChildren();if(!info||!info.project)return;
  try{const path=new URL(info.project).pathname;const r=await fetch('https://api.github.com/repos'+path+'/releases/latest');if(!r.ok)throw 0;const j=await r.json(),v=j.tag_name.replace(/^v/,'');
    if(v===info.version){m.textContent=t('updn');return}
    const a=document.createElement('a');a.href=j.html_url;a.target='_blank';a.textContent=t('updl')+v;m.append(a)}catch(e){m.textContent=t('upde')}};
$('file').onchange=async()=>{const f=$('file').files[0];fw=null;$('fwgo').disabled=true;$('warn').textContent='';if(!f)return;
  const b=new Uint8Array(await f.arrayBuffer()),r=checkFirmware(b,info);
  if(r.err){$('fwmsg').textContent=t('wrong')+t(r.err);return}
  fw=b;$('fwmsg').textContent=t('chk')+r.version;$('fwgo').disabled=false;
  if(info.version!=='dev'&&r.version!=='dev'&&r.version<info.version)$('warn').textContent=t('older')};
$('fwgo').onclick=()=>{if(!fw)return;$('fwgo').disabled=true;const x=new XMLHttpRequest();x.open('POST','/dongle/ota');x.setRequestHeader('Content-Type','application/octet-stream');
  x.upload.onprogress=e=>{$('fwmsg').textContent=t('send')+Math.round(e.loaded*100/e.total)+'%'};
  x.onload=()=>{$('fwmsg').textContent=x.status===200?t('ok'):t('fail')+x.responseText};
  x.onerror=()=>{$('fwmsg').textContent=t('lost')};x.send(fw)};
function drawNets(){const b=$('nets');b.replaceChildren();if(!nets.length){b.textContent=t('wscan');return}
  for(const n of nets){const x=document.createElement('button');x.className='net'+(n.ssid===$('wssid').value?' on':'');const a=document.createElement('span'),c=document.createElement('span');a.textContent=n.ssid;c.textContent=(n.lock?'🔒 ':'')+n.rssi+' dBm';x.append(a,c);x.onclick=()=>{$('wssid').value=n.ssid;drawNets();$('wpsk').focus()};b.append(x)}}
async function scan(){if(!info||saved)return;try{const j=await(await fetch(info.ap?'/config.json':'/dongle/wifi')).json();nets=j.aps.filter(a=>a.ssid).sort((a,b)=>b.rssi-a.rssi);drawNets()}catch(e){}}
$('wshow').onchange=()=>{$('wpsk').type=$('wshow').checked?'text':'password'};
$('wsave').onclick=async()=>{const s=$('wssid').value;if(!s){$('wmsg').textContent=t('wneed');return}
  try{const q=new URLSearchParams({ssid:s,psk:$('wpsk').value}),ap=info.ap;const r=ap?await fetch('/wifisave?'+q):await fetch('/dongle/wifi',{method:'POST',body:q});if(!r.ok)throw 0;saved=true;$('wmsg').textContent='';$('about').textContent=ap?t('wsaved'):t('wsaved2');$('wifi').style.display='none'}catch(e){$('wmsg').textContent=t('wfail')}};
texts();tick().then(scan);const timer=setInterval(tick,5000);setInterval(scan,info&&info.ap?5000:10000);
</script></html>)HTML";

void DongleCore::setup() {
  this->leds_pref_ = global_preferences->make_preference<uint8_t>(LEDS_PREF_HASH);
  uint8_t stored = 0;
  this->leds_enabled_.store(!(this->leds_pref_.load(&stored) && stored == LEDS_OFF));
#ifdef USE_WIFI
  // Результаты поиска нужны странице и после подключения к сети.
  if (wifi::global_wifi_component != nullptr)
    wifi::global_wifi_component->set_keep_scan_results(true);
#endif
  this->filter_pref_ = global_preferences->make_preference<uint32_t>(FILTER_PREF_HASH);
  uint32_t minutes = 0;
  if (this->filter_pref_.load(&minutes)) {
    this->filter_minutes_.store(minutes);
    this->filter_saved_minutes_ = minutes;
  }
  this->rollback_pref_ = global_preferences->make_preference<WifiRollback>(ROLLBACK_PREF_HASH);
  if (!this->rollback_pref_.load(&this->rollback_))
    this->rollback_.armed = false;
  this->base_->add_handler(this);
}

// Запись во flash только раз в час работы: при обрыве питания теряется не больше часа счёта.
void DongleCore::filter_loop_() {
  uint32_t now = millis();
  if (this->filter_reset_requested_.exchange(false)) {
    this->filter_minutes_.store(0);
    this->filter_saved_minutes_ = 0;
    uint32_t zero = 0;
    this->filter_pref_.save(&zero);
    global_preferences->sync();
  }
  if (now - this->filter_last_tick_ < 60000)
    return;
  this->filter_last_tick_ = now;
  if (!this->running_callback_ || !this->running_callback_())
    return;
  uint32_t minutes = this->filter_minutes_.load() + 1;
  this->filter_minutes_.store(minutes);
  if (minutes - this->filter_saved_minutes_ >= FILTER_SAVE_EVERY_MINUTES) {
    this->filter_saved_minutes_ = minutes;
    this->filter_pref_.save(&minutes);
    global_preferences->sync();
  }
}

void DongleCore::wifi_change_loop_() {
#ifdef USE_WIFI
  auto *wifi_component = wifi::global_wifi_component;
  if (wifi_component == nullptr)
    return;
  // Поиск идёт напрямую через драйвер и не разрывает подключение, результаты сохраняет ESPHome.
  if (this->scan_requested_.exchange(false) && wifi_component->is_connected() &&
      (this->last_scan_ == 0 || millis() - this->last_scan_ > SCAN_INTERVAL_MS)) {
    this->last_scan_ = millis();
    wifi_scan_config_t config{};
    config.show_hidden = false;
    config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    config.scan_time.active.min = 60;
    config.scan_time.active.max = 120;
    esp_err_t err = esp_wifi_scan_start(&config, false);
    if (err != ESP_OK)
      ESP_LOGW(TAG, "Поиск сетей: %s", esp_err_to_name(err));
  }
  if (this->wifi_change_requested_.exchange(false)) {
    // Текущая сеть становится запасной, затем перезагрузка на новую.
    WifiRollback backup{};
    wifi::WiFiAP current = wifi_component->get_sta();
    std::strncpy(backup.ssid, current.get_ssid().c_str(), sizeof(backup.ssid) - 1);
    std::strncpy(backup.password, current.get_password().c_str(), sizeof(backup.password) - 1);
    backup.armed = true;
    this->rollback_pref_.save(&backup);
    wifi_component->save_wifi_sta(this->new_ssid_.c_str(), this->new_password_.c_str());
    global_preferences->sync();
    ESP_LOGI(TAG, "Смена сети Wi-Fi, перезагрузка");
    this->reboot_at_ = millis();
    this->reboot_pending_.store(true);
    return;
  }
  if (!this->rollback_.armed)
    return;
  if (wifi_component->is_connected()) {
    this->rollback_.armed = false;
    this->rollback_pref_.save(&this->rollback_);
    global_preferences->sync();
    ESP_LOGI(TAG, "Новая сеть Wi-Fi подтверждена");
  } else if (millis() > ROLLBACK_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Новая сеть не подключилась, возврат на прежнюю");
    WifiRollback old = this->rollback_;
    this->rollback_.armed = false;
    this->rollback_pref_.save(&this->rollback_);
    wifi_component->save_wifi_sta(old.ssid, old.password);
    global_preferences->sync();
    App.safe_reboot();
  }
#endif
}

bool DongleCore::setup_network_active_() const {
#ifdef USE_CAPTIVE_PORTAL
  return captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active();
#else
  return false;
#endif
}

void DongleCore::set_marker(const char *marker) {
  this->marker_ = marker;
  const char *version = std::strstr(marker, "version=");
  this->expected_ = version != nullptr ? std::string(marker, version - marker) : std::string(marker);
}

// Прошивка одна на всех, поэтому ключ создаётся на самом донгле и хранится во flash.
void DongleCore::ensure_api_key_() {
#if defined(USE_API) && defined(USE_API_NOISE) && !defined(USE_API_NOISE_PSK_FROM_YAML)
  auto *server = api::global_api_server;
  if (server == nullptr)
    return;
  bool wanted = this->new_key_requested_.exchange(false);
  bool first_start = !wanted && !this->key_checked_ && !server->get_noise_ctx().has_psk();
  if (!wanted && (this->key_checked_ || server->get_noise_ctx().has_psk())) {
    this->key_checked_ = true;
    return;
  }
  noise::psk_t psk{};
  if (!random_bytes(psk.data(), psk.size())) {
    ESP_LOGE(TAG, "Нет случайных данных для ключа");
    return;
  }
  if (server->save_noise_psk(psk, true)) {
    this->key_checked_ = true;
    ESP_LOGI(TAG, "Создан новый ключ API");
    // Первый запуск или сброс: ключ нужно забрать со страницы, поэтому она открывается сама.
    if (first_start)
      this->open_window();
  }
#endif
}

void DongleCore::loop() {
  if (!this->server_started_) {
    this->server_started_ = true;
    this->base_->init();
  }
  this->ensure_api_key_();
  this->wifi_change_loop_();
  this->filter_loop_();
  if (this->reboot_pending_.load() && millis() - this->reboot_at_ > 1000) {
    App.safe_reboot();
  }
  if (this->leds_dirty_.exchange(false)) {
    uint8_t value = this->leds_enabled_.load() ? 1 : LEDS_OFF;
    this->leds_pref_.save(&value);
    global_preferences->sync();
  }
  // Пока Wi-Fi не настроен, страница нужна постоянно, поэтому время окна не идёт.
  if (this->setup_network_active_()) {
    this->touch_();
  } else if (this->open_.load() && millis() - this->last_activity_.load() > this->window_timeout_) {
    ESP_LOGI(TAG, "Окно закрыто по таймеру");
    this->open_.store(false);
  }
}

void DongleCore::dump_config() {
  ESP_LOGCONFIG(TAG, "Dongle core: окно %u с", (unsigned) (this->window_timeout_ / 1000));
}

void DongleCore::open_window() {
  this->touch_();
  this->open_.store(true);
  ESP_LOGI(TAG, "Окно открыто");
}

void DongleCore::close_window() {
  this->open_.store(false);
  ESP_LOGI(TAG, "Окно закрыто");
}

void DongleCore::toggle_window() {
  if (this->is_window_open()) {
    this->close_window();
  } else {
    this->open_window();
  }
}

void DongleCore::set_leds_enabled(bool enabled) {
  this->leds_enabled_.store(enabled);
  this->leds_dirty_.store(true);
}

bool DongleCore::canHandle(AsyncWebServerRequest *request) const {
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buffer);
  bool dongle = url.size() >= 7 && std::strncmp(url.c_str(), "/dongle", 7) == 0;
  // В режиме точки доступа отвечаем на всё, кроме запросов списка сетей и сохранения сети:
  // телефоны открывают страницу, запрашивая произвольные адреса.
  if (this->setup_network_active_())
    return dongle || (request->method() == HTTP_GET && url != "/config.json" && url != "/wifisave");
  if (!this->open_.load())
    return false;
  if (url == "/")
    return request->method() == HTTP_GET;
  return dongle;
}

void DongleCore::handleRequest(AsyncWebServerRequest *request) {
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  StringRef url = request->url_to(buffer);
  bool post = request->method() == HTTP_POST;

  for (auto *handler : this->extra_handlers_) {
    if (handler->canHandle(request)) {
      handler->handleRequest(request);
      return;
    }
  }

  // Запрос состояния действием не считается: забытая вкладка не должна держать окно открытым.
  if (url == "/dongle/state" && !post) {
    uint32_t idle = millis() - this->last_activity_.load();
    uint32_t left = idle < this->window_timeout_ ? (this->window_timeout_ - idle) / 1000 : 0;
    std::string json = "{\"left\":" + std::to_string(left) + this->state_json_() + "}";
    request->send(200, "application/json", json.c_str());
    return;
  }
#ifdef USE_WIFI
  if (url == "/dongle/wifi" && !post) {
    this->scan_requested_.store(true);
    std::string json = this->networks_json_();
    request->send(200, "application/json", json.c_str());
    return;
  }
  if (url == "/dongle/wifi" && post) {
    this->touch_();
    std::string ssid = request->arg("ssid");
    std::string password = request->arg("psk");
    if (ssid.empty() || ssid.size() > 32 || password.size() > 64 || this->wifi_change_requested_.load()) {
      request->send(400, "text/plain", "bad network");
      return;
    }
    this->new_ssid_ = ssid;
    this->new_password_ = password;
    this->wifi_change_requested_.store(true);
    request->send(200, "text/plain", "OK");
    return;
  }
#endif
  if (url == "/dongle/filter/reset" && post) {
    this->touch_();
    this->reset_filter();
    request->send(200, "application/json", "{}");
    return;
  }
  if (url == "/dongle/touch" && post) {
    this->touch_();
    request->send(200, "application/json", "{}");
    return;
  }
#if defined(USE_API) && defined(USE_API_NOISE) && !defined(USE_API_NOISE_PSK_FROM_YAML)
  if (url == "/dongle/key" && !post) {
    this->touch_();
    std::string json = "{\"key\":\"";
    if (api::global_api_server != nullptr && api::global_api_server->get_noise_ctx().has_psk()) {
      noise::psk_t psk{};
      api::global_api_server->get_noise_ctx().load_psk(psk);
      json += base64_encode(psk.data(), psk.size());
    }
    json += "\"}";
    request->send(200, "application/json", json.c_str());
    return;
  }
  if (url == "/dongle/key/new" && post) {
    this->touch_();
    this->new_key_requested_.store(true);
    request->send(200, "application/json", "{}");
    return;
  }
#endif
  if (url == "/dongle/ota" && post) {
    this->touch_();
    if (this->ota_error_ == nullptr && this->ota_backend_ == nullptr)
      this->ota_error_ = "no data";
    if (this->ota_error_ == nullptr && !this->ota_marker_found_)
      this->ota_fail_("not compatible");
    if (this->ota_error_ == nullptr) {
      ota::OTAResponseTypes result = this->ota_backend_->end();
      this->ota_backend_.reset();
      if (result != ota::OTA_RESPONSE_OK) {
        ESP_LOGE(TAG, "OTA end: %d", result);
        this->ota_error_ = "image check failed";
      }
    }
    if (this->ota_error_ != nullptr) {
      request->send(400, "text/plain", this->ota_error_);
      return;
    }
    request->send(200, "text/plain", "OK");
    this->reboot_at_ = millis();
    this->reboot_pending_.store(true);
    return;
  }
  if ((url == "/" || url == "/dongle" || this->setup_network_active_()) && !post) {
    this->touch_();
    request->send(200, "text/html; charset=utf-8", PAGE);
    return;
  }
  if (url == "/dongle/leds" && post) {
    this->touch_();
    this->set_leds_enabled(!this->leds_enabled());
    request->send(200, "application/json", "{}");
    return;
  }
  if (url == "/dongle/close" && post) {
    this->close_window();
    request->send(200, "application/json", "{}");
    return;
  }
  request->send(404, "text/plain", "Not found");
}

std::string DongleCore::networks_json_() {
  std::string out = "{\"aps\":[";
#ifdef USE_WIFI
  char escaped[32 * JSON_ESCAPE_MAX_EXPANSION + 1];
  wifi::ScanResultsLock lock(wifi::global_wifi_component);
  const auto &results = wifi::global_wifi_component->get_scan_result();
  bool first = true;
  for (const auto &scan : results) {
    bool with_auth = false;
    if (!wifi::should_show_scan_entry(results, scan, with_auth))
      continue;
    json_escape_into_buffer(std::span<char>(escaped, sizeof(escaped)), scan.get_ssid());
    if (!first)
      out += ',';
    first = false;
    out += "{\"ssid\":\"" + std::string(escaped) + "\",\"rssi\":" + std::to_string(scan.get_rssi()) +
           ",\"lock\":" + (with_auth ? "1" : "0") + "}";
  }
#endif
  out += "]}";
  return out;
}

std::string DongleCore::state_json_() {
  // Метка проверена при сборке и не содержит кавычек.
  std::string marker(this->marker_);
  auto part = [&marker](const char *key) {
    size_t start = marker.find(key);
    if (start == std::string::npos)
      return std::string();
    start += std::strlen(key);
    return marker.substr(start, marker.find('|', start) - start);
  };
  std::string ip;
  for (auto &address : network::get_ip_addresses()) {
    if (address.is_ip4() && address.is_set()) {
      char buf[network::IP_ADDRESS_BUFFER_SIZE];
      ip = address.str_to(buf);
      break;
    }
  }
  int rssi = 0;
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    rssi = wifi::global_wifi_component->wifi_rssi();
#endif
  bool link = this->link_callback_ && this->link_callback_();
  std::string ssid;
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected()) {
    char raw[wifi::SSID_BUFFER_SIZE];
    char escaped[32 * JSON_ESCAPE_MAX_EXPANSION + 1];
    json_escape_into_buffer(std::span<char>(escaped, sizeof(escaped)), StringRef(wifi::global_wifi_component->wifi_ssid_to(raw)));
    ssid = escaped;
  }
#endif
  std::string out = std::string(",\"leds\":") + (this->leds_enabled() ? "true" : "false");
  out += ",\"chip\":" + std::to_string(CONFIG_IDF_FIRMWARE_CHIP_ID);
  out += ",\"board\":\"" + part("board=") + "\",\"device\":\"" + part("device=") + "\",\"version\":\"" + part("version=") + "\"";
  out += ",\"name\":\"" + App.get_name() + "\",\"ip\":\"" + ip + "\",\"rssi\":" + std::to_string(rssi);
  out += ",\"up\":" + std::to_string(millis() / 1000) + ",\"link\":" + (link ? "true" : "false");
  out += ",\"project\":\"" + std::string(this->project_url_) + "\"";
  char mac[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  out += ",\"mac\":\"" + std::string(get_mac_address_pretty_into_buffer(mac)) + "\"";
  out += ",\"ssid\":\"" + ssid + "\"";
  out += ",\"filter\":" + std::to_string(this->filter_minutes_.load() / 60);
  out += ",\"flimit\":" + std::to_string(this->filter_limit_hours_);
  out += std::string(",\"ap\":") + (this->setup_network_active_() ? "true" : "false");
  return out;
}

void DongleCore::ota_fail_(const char *reason) {
  ESP_LOGE(TAG, "OTA отклонено: %s", reason);
  if (this->ota_backend_ != nullptr) {
    this->ota_backend_->abort();
    this->ota_backend_.reset();
  }
  this->ota_error_ = reason;
}

void DongleCore::ota_scan_(const uint8_t *data, size_t len) {
  const size_t size = this->expected_.size();
  const char *expected = this->expected_.c_str();
  for (size_t i = 0; i < len && !this->ota_marker_found_; i++) {
    if (static_cast<char>(data[i]) == expected[this->ota_match_]) {
      if (++this->ota_match_ == size)
        this->ota_marker_found_ = true;
    } else {
      this->ota_match_ = static_cast<char>(data[i]) == expected[0] ? 1 : 0;
    }
  }
}

void DongleCore::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  char buffer[AsyncWebServerRequest::URL_BUF_SIZE];
  if (request->url_to(buffer) != "/dongle/ota")
    return;

  if (index == 0) {
    if (this->ota_backend_ != nullptr)
      this->ota_backend_->abort();
    this->ota_backend_.reset();
    this->ota_error_ = nullptr;
    this->ota_match_ = 0;
    this->ota_marker_found_ = false;

    // Проверки до первой записи во flash.
    uint32_t magic = 0;
    if (len < IMAGE_HEADER_MIN) {
      this->ota_fail_("file too short");
      return;
    }
    std::memcpy(&magic, data + APP_DESC_OFFSET, sizeof(magic));
    if (data[0] != 0xE9 || magic != APP_DESC_MAGIC) {
      this->ota_fail_("not an application image");
      return;
    }
    if (static_cast<uint16_t>(data[12] | (data[13] << 8)) != CONFIG_IDF_FIRMWARE_CHIP_ID) {
      this->ota_fail_("wrong chip");
      return;
    }
    this->ota_backend_ = ota::make_ota_backend();
    ota::OTAResponseTypes result = this->ota_backend_->begin(total);
    if (result != ota::OTA_RESPONSE_OK) {
      ESP_LOGE(TAG, "OTA begin: %d", result);
      this->ota_fail_("cannot start update");
      return;
    }
    ESP_LOGI(TAG, "OTA начато, %u байт", (unsigned) total);
  }
  if (this->ota_error_ != nullptr || this->ota_backend_ == nullptr)
    return;
  this->ota_scan_(data, len);
  ota::OTAResponseTypes result = this->ota_backend_->write(data, len);
  if (result != ota::OTA_RESPONSE_OK) {
    ESP_LOGE(TAG, "OTA write: %d", result);
    this->ota_fail_("flash write failed");
  }
}

}  // namespace esphome::dongle_core
