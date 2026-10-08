# Wi-Fi 與藍牙

上課教材

- [Infineon PSoC™ Edge E84 AI Kit-23 教學Bluetooth → PSOC Edge Bluetooth LE FindMe](https://hackmd.io/6VBfRty_ShK-730xXv_x8A?view)

## 硬體基本介紹

搭載Wi-Fi 6 & BLE 5.4

支援Protocal

- Wi-Fi a/b/c/g/n/ac/ax
- BLE 5.4 BR/EDR/LE

藍芽比較表

<!-- |           | BR  | EDR | LE  |
| --------- | --- | --- | --- |
| 類別      | -   | -   | -   |
| 空氣速率  | -   | -   | -   |
| 傳輸量    | -   | -   | -   |
| IOS相容性 | -   | -   | -   |
| 互通性    | -   | -   | -   |
| 設定檔    | -   | -   | -   |
| 連線      | 1 個 master 最多帶 7 個 slave   || 可以同時連很多裝置，也能做廣播或 mesh   |
| 耗電      | -   | -   | -   | -->

<table>
  <tr>
    <th></th>
    <th>BR</th>
    <th>EDR</th>
    <th>LE</th>
  </tr>
  <tr>
    <td>類別</td>
    <td colspan="2">傳統藍牙</td>
    <td>低功耗藍牙</td>
  </tr>
  <tr>
    <td>空氣速率</td>
    <td>1 Mbps</td>
    <td>3 Mbps</td>
    <td>2 Mbps PHY</td>
  </tr>
  <tr>
    <td>傳輸量</td>
    <td>0.7 Mbps</td>
    <td>2.1 Mbps</td>
    <td>1.4 Mbps PHY</td>
  </tr>
  <tr>
    <td>IOS相容性</td>
    <td colspan="2">不相容</td>
    <td>相容</td>
  </tr>
  <tr>
    <td>互通性</td>
    <td>和EDR互通</td>
    <td>和BR互通</td>
    <td>不互通</td>
  </tr>
  <tr>
    <td>Profile</td>
    <td colspan="2">SPP、A2DP、HFP、HID</td>
    <td>GATT、LE Audio</td>
  </tr>
  <tr>
    <td>連線</td>
    <td colspan="2">1 個 master 最多帶 7 個 slave</td>
    <td>可以同時連很多裝置，也能做廣播或 mesh</td>
  </tr>
  <tr>
    <td>耗電</td>
    <td colspan="2">高</td>
    <td>低</td>
  </tr>
</table>

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
