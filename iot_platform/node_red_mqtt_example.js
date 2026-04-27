[
    {
        "id": "7b09e8a82c3090e7",
        "type": "tab",
        "label": "流程2",
        "disabled": false,
        "info": "",
        "env": []
    },
    {
        "id": "54fc4ea0dd138a68",
        "type": "debug",
        "z": "7b09e8a82c3090e7",
        "name": "debug 1",
        "active": true,
        "tosidebar": true,
        "console": false,
        "tostatus": false,
        "complete": "false",
        "statusVal": "",
        "statusType": "auto",
        "x": 900,
        "y": 300,
        "wires": []
    },
    {
        "id": "7ac61497b346f0d7",
        "type": "mqtt in",
        "z": "7b09e8a82c3090e7",
        "name": "",
        "topic": "mqtt_hihr4n73/#",
        "qos": "2",
        "datatype": "auto-detect",
        "broker": "1e7e45d079a1ce43",
        "nl": false,
        "rap": true,
        "rh": 0,
        "inputs": 0,
        "x": 520,
        "y": 300,
        "wires": [
            [
                "54fc4ea0dd138a68"
            ]
        ]
    },
    {
        "id": "4b81bcf9725ca472",
        "type": "mqtt out",
        "z": "7b09e8a82c3090e7",
        "name": "",
        "topic": "",
        "qos": "",
        "retain": "",
        "respTopic": "",
        "contentType": "",
        "userProps": "",
        "correl": "",
        "expiry": "",
        "broker": "1e7e45d079a1ce43",
        "x": 670,
        "y": 440,
        "wires": []
    },
    {
        "id": "14d58b3a275503d7",
        "type": "inject",
        "z": "7b09e8a82c3090e7",
        "name": "",
        "props": [
            {
                "p": "payload"
            },
            {
                "p": "topic",
                "vt": "str"
            }
        ],
        "repeat": "10",
        "crontab": "",
        "once": false,
        "onceDelay": 0.1,
        "topic": "",
        "payload": "",
        "payloadType": "date",
        "x": 220,
        "y": 440,
        "wires": [
            [
                "9bee3a68dcf4bb27"
            ]
        ]
    },
    {
        "id": "9bee3a68dcf4bb27",
        "type": "function",
        "z": "7b09e8a82c3090e7",
        "name": "function 1",
        "func": "// 1. 參數化定義 (未來要換人測試，只要改這三行)\nconst USER_ID = \"mqtt_hihr4n73\";   // 你的租戶 ID\nconst DEVICE_ID = \"device_v1\";     // 設備編號\nconst SENSOR_TYPE = \"temperature\"; // 感測器類型\n\n// 2. 自動組合 Topic (嚴格遵守 +/+/+ 規則)\n// 使用 ES6 樣板字面值，避免手動拼湊字串出錯\nmsg.topic = `${USER_ID}/${DEVICE_ID}/${SENSOR_TYPE}`;\n\n// 3. 產生模擬數據\nlet temp = (Math.random() * (35 - 20) + 20).toFixed(1);\n// (可選) 順便多加一個濕度，讓圖表更豐富\nlet humi = (Math.random() * (90 - 50) + 50).toFixed(1); \n\n// 4. 設定標準化 Payload\nmsg.payload = {\n    \"temperature\": parseFloat(temp),\n    \"humidity\": parseFloat(humi)\n};\n\nreturn msg;",
        "outputs": 1,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 417,
        "y": 440,
        "wires": [
            [
                "4b81bcf9725ca472"
            ]
        ]
    },
    {
        "id": "1e7e45d079a1ce43",
        "type": "mqtt-broker",
        "name": "mqtt_broke",
        "broker": "broker.makdev.net",
        "port": 1883,
        "clientid": "",
        "autoConnect": true,
        "usetls": false,
        "protocolVersion": 4,
        "keepalive": 60,
        "cleansession": true,
        "autoUnsubscribe": true,
        "birthTopic": "",
        "birthQos": "0",
        "birthRetain": "false",
        "birthPayload": "",
        "birthMsg": {},
        "closeTopic": "",
        "closeQos": "0",
        "closeRetain": "false",
        "closePayload": "",
        "closeMsg": {},
        "willTopic": "",
        "willQos": "0",
        "willRetain": "false",
        "willPayload": "",
        "willMsg": {},
        "userProps": "",
        "sessionExpiry": ""
    }
]