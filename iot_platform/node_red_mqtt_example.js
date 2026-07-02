[
    {
        "id": "40550e52ca5b88b6",
        "type": "tab",
        "label": "流程3",
        "disabled": false,
        "info": "",
        "env": []
    },
    {
        "id": "33cdb2c9fdaca9a8",
        "type": "inject",
        "z": "40550e52ca5b88b6",
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
        "x": 340,
        "y": 280,
        "wires": [
            [
                "0db1de94156c8dd0"
            ]
        ]
    },
    {
        "id": "0db1de94156c8dd0",
        "type": "function",
        "z": "40550e52ca5b88b6",
        "name": "function 2",
        "func": "// 1. 參數化定義 (未來要換人測試，只要改這三行)\nconst USER_ID = \"your_account\";   // 你的租戶 ID\nconst DEVICE_ID = \"device_v1\"; // 設備編號\nconst SENSOR_TYPE = \"env\"; // 感測器類型\n\n// 2. 自動組合 Topic (嚴格遵守 +/+/+ 規則)\n// 使用 ES6 樣板字面值，避免手動拼湊字串出錯\nmsg.topic = `${USER_ID}/${DEVICE_ID}/${SENSOR_TYPE}`;\n\n// 3. 產生模擬數據\nlet temp = (Math.random() * (35 - 20) + 20).toFixed(1);\n// (可選) 順便多加一個濕度，讓圖表更豐富\nlet humi = (Math.random() * (90 - 50) + 50).toFixed(1); \n\n// 4. 設定標準化 Payload\nmsg.payload = {\n    \"temperature\": parseFloat(temp),\n    \"humidity\": parseFloat(humi)\n};\n\nreturn msg;",
        "outputs": 1,
        "timeout": 0,
        "noerr": 0,
        "initialize": "",
        "finalize": "",
        "libs": [],
        "x": 537,
        "y": 280,
        "wires": [
            [
                "5254906605963f44"
            ]
        ]
    },
    {
        "id": "87542857c87f4b3c",
        "type": "comment",
        "z": "40550e52ca5b88b6",
        "name": "輸出 json 格式",
        "info": "{\"temperature\":23.5,\"humidity\":56.4}",
        "x": 530,
        "y": 340,
        "wires": []
    },
    {
        "id": "5254906605963f44",
        "type": "mqtt out",
        "z": "40550e52ca5b88b6",
        "name": "",
        "topic": "",
        "qos": "",
        "retain": "",
        "respTopic": "",
        "contentType": "",
        "userProps": "",
        "correl": "",
        "expiry": "",
        "broker": "e67d8bc364090af8",
        "x": 770,
        "y": 280,
        "wires": []
    },
    {
        "id": "e67d8bc364090af8",
        "type": "mqtt-broker",
        "name": "makdev.net mqtt",
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