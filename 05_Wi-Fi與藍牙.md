# Wi-Fi 與藍牙

上課教材

- [Infineon PSoC™ Edge E84 AI Kit-23 教學Bluetooth → PSOC Edge Bluetooth LE FindMe](https://hackmd.io/6VBfRty_ShK-730xXv_x8A?view)

## 硬體基本介紹

搭載Wi-Fi 6 & BLE 5.4

支援Protocal

- Wi-Fi a/b/c/g/n/ac/ax
- BLE 5.4 BR/EDR/LE

PHY data rate

- Wi-Fi 143 Mbps
- BLE 3 Mbps
- BLE LE 2 Mbps

## Sample Code

本教學使用 PSoC™ Edge E84 AI Kit 的官方範例，直接在Eclipse加入新專案即可。

```
Bluetooth → PSOC Edge Bluetooth LE FindMe
```

## 藍芽連線流程

```
1. Advertising：E84 對外廣播
2. Connection：手機連線到 E84
3. GATT Service：手機看到 E84 提供的 BLE 服務
4. GATT Write：手機寫入資料給 E84
5. LED / Alert：E84 根據手機寫入的資料做出反應
```
