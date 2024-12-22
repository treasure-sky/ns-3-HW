import re

def parse_time_tx(line: str) -> float:
    """
    TX 라인에서 'Time: +x.xxxxs' 형태의 시간을 초 단위(float)로 파싱.
    """
    match = re.search(r"Time:\s*\+([\d.e]+)s", line)
    if match:
        return float(match.group(1))
    return None

def parse_time_rx(line: str) -> float:
    """
    RX 라인에서 'RXtime: +x.xxxxxexxns' 형태의 시간을 초 단위(float)로 파싱.
    ns(나노초)를 초 단위로 변환한다.
    """
    match = re.search(r"RXtime:\s*\+([\d.e+\-]+)ns", line)
    if match:
        return float(match.group(1)) / 1e9
    return None

def parse_bytes(line: str) -> int:
    """
    라인에서 'XXXX bytes' 형태의 정수 바이트 수를 추출.
    """
    match = re.search(r"(\d+)\s*bytes", line)
    if match:
        return int(match.group(1))
    return 0

def calculate_throughput(file_path: str) -> float:
    """
    주어진 로그 파일에서 가장 이른 TX 시각과 가장 늦은 RX 시각을 찾은 뒤, 
    전체 구간에서 전송된 총 바이트 수를 통해 throughput을 계산한다.
    """
    tx_times = []  # [time_in_seconds, ...]
    rx_records = []    # (time_in_seconds, bytes)

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()

            # TX 라인이면
            if "TraceDelay TX" in line:
                t = parse_time_tx(line)
                if t is not None:
                    tx_times.append(t)

            # RX 라인이면
            elif "TraceDelay: RX" in line:
                t = parse_time_rx(line)
                b = parse_bytes(line)
                if t is not None:
                    rx_records.append((t, b))

    # 가장 이른 TX 시각과 가장 늦은 RX 시각 구하기
    if not tx_times or not rx_records:
        print("TX 혹은 RX 기록이 충분하지 않아 throughput을 계산할 수 없습니다.")
        return 0.0

    earliest_tx_time = min(tx_times)
    latest_rx_time = max(t[0] for t in rx_records)

    if latest_rx_time <= earliest_tx_time:
        print("가장 늦은 RX 시각이 가장 이른 TX 시각보다 같거나 빠릅니다.")
        return 0.0

    # 전체 구간 내(earliest_tx_time ~ latest_rx_time)에 발생한 RX 바이트 합산
    total_bytes = sum(r[1] for r in rx_records)

    total_time = latest_rx_time - earliest_tx_time
    throughput = total_bytes / total_time  # bytes/s
    return throughput

if __name__ == "__main__":
    log_file_path = "/Users/jinholee/WORK/School/3-2/네트워크/HW2 ns-3 시뮬/ns-allinone-3.43/ns-3.43/scratch/size1500.log"
    
    result = calculate_throughput(log_file_path)
    if result > 0:
        print(f"총 throughput: {result:.2f} bytes/s")
    else:
        print("throughput 계산 실패 혹은 0 bytes/s")
