#!/usr/bin/env python3
"""
UDP Client for testing
可用來測試 Arduino UDP Server 或其他 UDP 伺服器
"""

import socket
import time

# 目標伺服器設定
TARGET_IP = '192.168.1.177'  # Arduino IP (請修改為您的 Arduino IP)
TARGET_PORT = 5000            # Arduino 監聽的端口

# 本地設定
LOCAL_PORT = 6000

def main():
    # 建立 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', LOCAL_PORT))
    sock.settimeout(2.0)  # 設定 2 秒逾時
    
    print(f"=== UDP Client Started ===")
    print(f"Target: {TARGET_IP}:{TARGET_PORT}")
    print(f"Local port: {LOCAL_PORT}\n")
    
    counter = 0
    
    try:
        while True:
            counter += 1
            
            # 發送訊息
            message = f"Hello from Python! Count: {counter}"
            print(f"Sending: {message}")
            sock.sendto(message.encode('utf-8'), (TARGET_IP, TARGET_PORT))
            
            # 嘗試接收回覆
            try:
                data, server_address = sock.recvfrom(1024)
                reply = data.decode('utf-8', errors='ignore')
                print(f"Received reply from {server_address[0]}:{server_address[1]}")
                print(f"  Reply: {reply}\n")
            except socket.timeout:
                print("  No reply (timeout)\n")
            
            # 等待 5 秒再發送下一則訊息
            time.sleep(5)
            
    except KeyboardInterrupt:
        print("\n\nClient stopped by user")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()
        print("Socket closed")

if __name__ == "__main__":
    main()
