#!/usr/bin/env python3
"""
UDP Server for W5500 Arduino Client
接收來自 Arduino 的 UDP 訊息並回覆
"""

import socket
import datetime

# 伺服器設定
SERVER_IP = '0.0.0.0'  # 監聽所有網路介面
SERVER_PORT = 5000

def main():
    # 建立 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    
    print(f"=== UDP Server Started ===")
    print(f"Listening on {SERVER_IP}:{SERVER_PORT}")
    print(f"Waiting for Arduino messages...\n")
    
    try:
        while True:
            # 接收資料 (buffer size 1024 bytes)
            data, client_address = sock.recvfrom(1024)
            
            # 解碼並顯示接收到的訊息
            message = data.decode('utf-8', errors='ignore')
            timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            
            print(f"[{timestamp}] Received from {client_address[0]}:{client_address[1]}")
            print(f"  Message: {message}")
            
            # 發送回覆給 Arduino
            reply = f"Server received: {message}"
            sock.sendto(reply.encode('utf-8'), client_address)
            print(f"  Replied: {reply}\n")
            
    except KeyboardInterrupt:
        print("\n\nServer stopped by user")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Socket closed")

if __name__ == "__main__":
    main()
