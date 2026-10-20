#!/usr/bin/env python3
"""
ESP32 Automatic Upload Controller v2.0
Automatiza firmware + SPIFFS upload via Arduino (HoodLoader2)

Uso:
    python esp32_uploader.py --port COM6 --action upload      # Solo firmware
    python esp32_uploader.py --port COM6 --action uploadfs    # Solo SPIFFS
    python esp32_uploader.py --port COM6 --action all         # Firmware + SPIFFS
    python esp32_uploader.py --port COM6 --status             # Ver estado

Requiere: pip install pyserial
"""

import serial
import serial.tools.list_ports
import subprocess
import time
import sys
import os
import re
import argparse
from pathlib import Path

# ── Configuración ──────────────────────────────────────────────
PIO_ENV = "heltec_wifi_kit_32"
BAUD_RATE = 9600
PROGRESS_BAR_WIDTH = 40

# ── Utilidades de consola ──────────────────────────────────────
def clear_line():
    """Borrar línea actual del terminal"""
    sys.stdout.write("\r" + " " * 80 + "\r")
    sys.stdout.flush()

def print_progress(label, percent):
    """Imprimir barra de progreso en la misma línea"""
    filled = int(PROGRESS_BAR_WIDTH * percent / 100)
    bar = "█" * filled + "░" * (PROGRESS_BAR_WIDTH - filled)
    clear_line()
    sys.stdout.write(f"\r  {label} [{bar}] {percent:3d}%")
    sys.stdout.flush()

def print_phase(emoji, text):
    """Imprimir encabezado de fase"""
    print(f"\n{'─' * 50}")
    print(f"  {emoji} {text}")
    print(f"{'─' * 50}")

# ── Auto-detección de puertos ──────────────────────────────────
def detect_ports():
    """Detectar Arduino y ESP32 automáticamente"""
    ports = list(serial.tools.list_ports.comports())
    arduino_port = None
    esp32_port = None
    
    for p in ports:
        desc = p.description.lower()
        # ESP32 Heltec usa chip CP210x
        if "cp210" in desc or "silicon labs" in desc:
            esp32_port = p.device
        # Arduino Mega / HoodLoader2 suele ser "USB Serial" o "Arduino"
        elif "arduino" in desc or "usb" in desc or "ch340" in desc or "serie" in desc:
            arduino_port = p.device
    
    return arduino_port, esp32_port

def list_ports():
    """Listar puertos disponibles"""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("  ❌ No se detectaron puertos COM")
        return
    print("  Puertos detectados:")
    for p in ports:
        print(f"    {p.device}: {p.description}")

# ── Controlador Arduino ───────────────────────────────────────
class ESP32Controller:
    def __init__(self, arduino_port, baud_rate=BAUD_RATE):
        self.arduino_port = arduino_port
        self.baud_rate = baud_rate
        self.arduino = None
        
    def connect(self):
        """Conectar al Arduino controlador"""
        try:
            print(f"  🔌 Conectando al Arduino en {self.arduino_port}...")
            self.arduino = serial.Serial(
                self.arduino_port, 
                self.baud_rate, 
                timeout=2,
                dsrdtr=False,   # Evitar reset por DTR (HoodLoader2)
                rtscts=False
            )
            time.sleep(2)  # Esperar inicialización
            self.arduino.reset_input_buffer()
            
            # Drenar mensajes de bienvenida
            self._drain(1.5)
            print(f"  ✅ Arduino conectado en {self.arduino_port}")
            return True
            
        except serial.SerialException as e:
            print(f"  ❌ Error: {e}")
            return False
    
    def _drain(self, timeout_sec=1.0):
        """Drenar buffer serial sin bloquear"""
        end = time.time() + timeout_sec
        while time.time() < end:
            if self.arduino and self.arduino.in_waiting > 0:
                try:
                    self.arduino.readline()
                except:
                    break
            else:
                time.sleep(0.05)
    
    def send_command(self, command, timeout=8):
        """Enviar comando y esperar respuesta OK/ERROR"""
        if not self.arduino:
            return False
            
        self.arduino.reset_input_buffer()
        self.arduino.write(f"{command}\n".encode())
        self.arduino.flush()
        time.sleep(0.3)
        
        start = time.time()
        while time.time() - start < timeout:
            if self.arduino.in_waiting > 0:
                try:
                    line = self.arduino.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"    Arduino: {line}")
                        if "OK" in line.upper() or "LISTO" in line.upper():
                            return True
                        if "ERROR" in line.upper():
                            return False
                except:
                    pass
            else:
                time.sleep(0.05)
        
        print(f"    ⚠️ Sin respuesta tras {timeout}s (continuando...)")
        return True  # Continuar aunque no responda
    
    def enter_download_mode(self):
        """Poner ESP32 en modo download (GPIO0=LOW + reset)"""
        print("  📥 Entrando en modo download...")
        result = self.send_command("DOWNLOAD")
        time.sleep(1.5)  # Esperar a que el ESP32 esté listo
        return result
    
    def release(self):
        """Liberar ESP32 a modo normal"""
        print("  🔄 Liberando ESP32 a modo normal...")
        return self.send_command("RELEASE")
    
    def disconnect(self):
        """Cerrar conexión serial"""
        if self.arduino:
            try:
                self.arduino.close()
            except:
                pass
            self.arduino = None

# ── Ejecución PlatformIO con progreso ─────────────────────────
def run_pio_with_progress(action):
    """
    Ejecutar PlatformIO mostrando progreso en tiempo real.
    Parsea la salida de esptool.py para extraer el porcentaje.
    """
    if action == 'upload':
        cmd = ['pio', 'run', '-e', PIO_ENV, '-t', 'upload']
        label = "Firmware"
    elif action == 'uploadfs':
        cmd = ['pio', 'run', '-e', PIO_ENV, '-t', 'uploadfs']
        label = "SPIFFS"
    else:
        print(f"  ❌ Acción desconocida: {action}")
        return False
    
    print(f"  ▶️  {' '.join(cmd)}")
    
    # Regex para capturar progreso de esptool: "Writing at 0x000... (33 %)"
    progress_re = re.compile(r'\((\d+)\s*%\)')
    # Regex para fases de compilación
    compiling_re = re.compile(r'Compiling\s')
    linking_re = re.compile(r'Linking|Archiving.*libFramework|Retrieving maximum')
    size_re = re.compile(r'(RAM|Flash):\s+\[.*\]\s+(\d+\.?\d*%)')
    success_re = re.compile(r'\[SUCCESS\]')
    failed_re = re.compile(r'\[FAILED\]|Error \d+')
    
    phase = "compile"  # compile -> link -> upload
    compile_count = 0
    last_percent = -1
    
    try:
        # Forzar output sin buffer para PlatformIO (usa Python internamente)
        env = os.environ.copy()
        env["PYTHONUNBUFFERED"] = "1"
        env["PYTHONIOENCODING"] = "utf-8"
        
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,                 # Sin buffer
            cwd=Path.cwd(),
            env=env
        )
        
        # Usar readline() en loop (NO for-in que buferea internamente)
        while True:
            raw = proc.stdout.readline()
            if not raw and proc.poll() is not None:
                break
            line = raw.decode('utf-8', errors='replace').strip()
            if not line:
                continue
            
            # ── Fase de compilación ──
            if phase == "compile":
                if compiling_re.search(line):
                    compile_count += 1
                    # Estimar progreso de compilación (aprox 80 archivos)
                    pct = min(compile_count * 100 // 80, 95)
                    if pct != last_percent:
                        print_progress(f"📦 Compilando {label}", pct)
                        last_percent = pct
                    continue
                    
                if linking_re.search(line):
                    print_progress(f"📦 Compilando {label}", 100)
                    print()  # Nueva línea
                    phase = "link"
                    continue
            
            # ── Fase de enlazado / tamaño ──
            if phase == "link":
                m = size_re.search(line)
                if m:
                    print(f"  💾 {m.group(1)}: {m.group(2)}")
                    continue
                if "Uploading" in line or "esptool" in line.lower() or "Writing" in line:
                    phase = "upload"
                    last_percent = -1
                    # NO continue, caer al parser de upload
            
            # ── Fase de upload (esptool) ──
            if phase == "upload" or phase == "link":
                m = progress_re.search(line)
                if m:
                    pct = int(m.group(1))
                    if pct != last_percent:
                        print_progress(f"🚀 Subiendo {label}", pct)
                        last_percent = pct
                    continue
            
            # ── Resultado final ──
            if success_re.search(line):
                if phase == "upload" and last_percent < 100:
                    print_progress(f"🚀 Subiendo {label}", 100)
                print()
                print(f"  ✅ {label}: Upload exitoso!")
                proc.wait()
                return True
                
            if failed_re.search(line):
                print()
                print(f"  ❌ {label}: {line}")
                proc.wait()
                return False
        
        # Esperar a que termine el proceso
        proc.wait()
        
        if proc.returncode == 0:
            print()
            print(f"  ✅ {label} completado")
            return True
        else:
            print()
            print(f"  ❌ {label} falló (código: {proc.returncode})")
            return False
            
    except FileNotFoundError:
        print("  ❌ PlatformIO (pio) no encontrado en el PATH")
        return False
    except Exception as e:
        print(f"\n  ❌ Error: {e}")
        return False

# ── Flujo principal ───────────────────────────────────────────
def do_upload(controller, action):
    """Ejecutar una acción de upload completa (download mode → pio → release)"""
    label = "Firmware" if action == "upload" else "SPIFFS/LittleFS"
    print_phase("📡", f"Subiendo {label}")
    
    # 1. Modo download
    if not controller.enter_download_mode():
        print("  ❌ No se pudo entrar en modo download")
        return False
    
    # 2. Ejecutar PlatformIO con progreso
    success = run_pio_with_progress(action)
    
    # 3. Liberar ESP32
    controller.release()
    time.sleep(1)
    
    return success

def main():
    parser = argparse.ArgumentParser(
        description='ESP32 Upload Controller v2.0 - Upload automático via Arduino',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Ejemplos:
  python esp32_uploader.py --action all            # Firmware + SPIFFS (auto-detect ports)
  python esp32_uploader.py --port COM6 --action upload   # Solo firmware
  python esp32_uploader.py --status                # Ver estado Arduino
  python esp32_uploader.py --list                  # Listar puertos COM
        """
    )
    parser.add_argument('--port', default=None, 
                        help='Puerto COM del Arduino (auto-detecta si no se especifica)')
    parser.add_argument('--action', choices=['upload', 'uploadfs', 'all'], default='all',
                        help='upload=firmware, uploadfs=SPIFFS, all=ambos (default: all)')
    parser.add_argument('--status', action='store_true', help='Solo ver estado del Arduino')
    parser.add_argument('--list', action='store_true', help='Listar puertos COM disponibles')
    
    args = parser.parse_args()
    
    print()
    print("  🚀 ESP32 Automatic Upload Controller v2.0")
    print("  ==========================================")
    
    # Listar puertos
    if args.list:
        list_ports()
        return 0
    
    # Auto-detectar puertos si no se especificó
    arduino_port = args.port
    if not arduino_port:
        print("  🔍 Auto-detectando puertos...")
        arduino_port, esp32_port = detect_ports()
        if arduino_port:
            print(f"    Arduino detectado: {arduino_port}")
        if esp32_port:
            print(f"    ESP32 detectado:   {esp32_port}")
        if not arduino_port:
            print("  ❌ No se detectó el Arduino. Usa --port COMx o --list")
            list_ports()
            return 1
    
    # Crear controlador
    controller = ESP32Controller(arduino_port)
    
    try:
        # Conectar
        if not controller.connect():
            return 1
        
        # Solo status
        if args.status:
            controller.send_command("STATUS")
            return 0
        
        start_time = time.time()
        results = {}
        
        # Determinar qué acciones ejecutar
        actions = []
        if args.action == 'all':
            actions = ['upload', 'uploadfs']
        else:
            actions = [args.action]
        
        # Ejecutar cada acción
        for action in actions:
            results[action] = do_upload(controller, action)
        
        # ── Resumen final ──
        elapsed = time.time() - start_time
        mins = int(elapsed // 60)
        secs = int(elapsed % 60)
        
        print_phase("📊", "Resumen")
        all_ok = True
        for action, success in results.items():
            label = "Firmware" if action == "upload" else "SPIFFS"
            icon = "✅" if success else "❌"
            print(f"  {icon} {label}: {'OK' if success else 'FALLÓ'}")
            if not success:
                all_ok = False
        
        print(f"  ⏱️  Tiempo total: {mins}m {secs}s")
        
        if all_ok:
            print("\n  🎉 ¡Todo completado exitosamente!")
            print("  💡 El ESP32 debería estar reiniciándose ahora...")
            return 0
        else:
            print("\n  ⚠️  Algunas operaciones fallaron")
            return 1
            
    except KeyboardInterrupt:
        print("\n\n  ⚠️  Interrumpido por usuario")
        controller.release()
        return 1
        
    except Exception as e:
        print(f"\n  ❌ Error inesperado: {e}")
        try:
            controller.release()
        except:
            pass
        return 1
        
    finally:
        controller.disconnect()

if __name__ == "__main__":
    sys.exit(main())