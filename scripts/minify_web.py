#!/usr/bin/env python3
"""
Script de minificación automática para archivos web
Se ejecuta como pre-build hook de PlatformIO
"""

import os
import re
import gzip
from pathlib import Path

# Determinar el directorio del proyecto
try:
    # Cuando se ejecuta desde PlatformIO
    Import("env")
    PROJECT_DIR = Path(env.get("PROJECT_DIR"))
except:
    # Cuando se ejecuta manualmente
    PROJECT_DIR = Path(__file__).parent.parent

DATA_WEB_DIR = PROJECT_DIR / "data" / "web"

def _extract_strings(content):
    """
    Extrae todas las cadenas de texto (single, double, template literals)
    y las reemplaza con placeholders para protegerlas durante la minificación.
    Retorna (content_con_placeholders, lista_de_strings).
    """
    strings = []
    result = []
    i = 0
    length = len(content)
    
    while i < length:
        ch = content[i]
        
        # Detectar inicio de string con comillas simples o dobles
        if ch in ('"', "'"):
            quote = ch
            start = i
            i += 1
            while i < length:
                if content[i] == '\\':
                    i += 2  # saltar caracter escapado
                elif content[i] == quote:
                    i += 1
                    break
                else:
                    i += 1
            strings.append(content[start:i])
            result.append(f'__STR{len(strings)-1}__')
            continue
        
        # Detectar template literals (backticks) - manejo simple sin nesting
        if ch == '`':
            start = i
            i += 1
            depth = 0
            while i < length:
                if content[i] == '\\':
                    i += 2
                elif content[i] == '$' and i + 1 < length and content[i + 1] == '{':
                    depth += 1
                    i += 2
                elif content[i] == '{' and depth > 0:
                    depth += 1
                    i += 1
                elif content[i] == '}' and depth > 0:
                    depth -= 1
                    i += 1
                elif content[i] == '`':
                    i += 1
                    break
                else:
                    i += 1
            strings.append(content[start:i])
            result.append(f'__STR{len(strings)-1}__')
            continue
        
        result.append(ch)
        i += 1
    
    return ''.join(result), strings


def _restore_strings(content, strings):
    """Restaura las cadenas protegidas desde sus placeholders."""
    for idx, s in enumerate(strings):
        content = content.replace(f'__STR{idx}__', s)
    return content


def minify_js(content):
    """
    Minificación AGRESIVA de JavaScript
    Elimina: comentarios, espacios innecesarios, líneas vacías, múltiples espacios
    Protege strings para no romper su contenido.
    """
    # Eliminar comentarios de bloque /* ... */
    content = re.sub(r'/\*[\s\S]*?\*/', '', content)
    
    # Eliminar comentarios de línea //
    # Cuidado: no eliminar // dentro de strings (se protegen después)
    content = re.sub(r'//[^\n]*', '', content)
    
    # Eliminar líneas vacías múltiples
    content = re.sub(r'\n\s*\n+', '\n', content)
    
    # Unir todo en una sola línea primero
    lines = content.split('\n')
    content = ' '.join(line.strip() for line in lines if line.strip())
    
    # Proteger strings antes de manipular espacios/keywords
    content, saved_strings = _extract_strings(content)
    
    # Eliminar espacios alrededor de operadores y símbolos
    content = re.sub(r'\s*([{}\[\]();:,=+\-*/<>!&|?])\s*', r'\1', content)
    
    # NOTA: NO se necesita restauración de espacios después de keywords.
    # El regex de operadores solo elimina espacios adyacentes a operadores
    # específicos ({,},(,),;,:,=,+,-,etc.), NO espacios entre palabras.
    # Ejemplo: "return true" → espacio preservado (no hay operador entre ellos)
    #          "if (x)"     → "if(x)" (espacio antes de ( eliminado, válido en JS)
    #          "const x=5"  → espacio entre const y x preservado
    
    # Eliminar espacios múltiples
    content = re.sub(r' +', ' ', content)
    
    # Restaurar strings originales
    content = _restore_strings(content, saved_strings)
    return content.strip()

def minify_css(content):
    """
    Minificación AGRESIVA de CSS
    """
    # Eliminar comentarios
    content = re.sub(r'/\*[\s\S]*?\*/', '', content)
    
    # Unir todo en una sola línea
    lines = content.split('\n')
    content = ' '.join(line.strip() for line in lines if line.strip())
    
    # Eliminar espacios alrededor de símbolos
    content = re.sub(r'\s*([{};:,>+~])\s*', r'\1', content)
    
    # Eliminar espacios múltiples
    content = re.sub(r' +', ' ', content)
    
    # Eliminar espacios antes de !
    content = re.sub(r' !', '!', content)
    
    return content.strip()

def minify_html(content):
    """
    Minificación AGRESIVA de HTML
    Preserva scripts y estilos inline
    """
    # Eliminar comentarios HTML (pero no los condicionales de IE)
    content = re.sub(r'<!--(?!\[if)[\s\S]*?-->', '', content)
    
    # Eliminar espacios entre tags
    content = re.sub(r'>\s+<', '><', content)
    
    # Eliminar líneas vacías y espacios múltiples
    content = re.sub(r'\n\s*\n+', '', content)
    content = re.sub(r'\s+', ' ', content)
    
    # Eliminar espacios al inicio/final
    return content.strip()

def process_file(src_path):
    """
    Procesa un archivo desde /src y genera versión minificada en parent
    Estructura: data/web/dashboard/src/dashboard.js → data/web/dashboard/dashboard.min.js
    """
    try:
        with open(src_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_size = len(content)
        ext = src_path.suffix.lower()
        
        # Solo minificar JS, CSS y HTML
        if ext not in ['.js', '.css', '.html']:
            return original_size, original_size, False, "", 0
        
        # Aplicar minificación según tipo
        if ext == '.js':
            minified_content = minify_js(content)
        elif ext == '.css':
            minified_content = minify_css(content)
        elif ext == '.html':
            minified_content = minify_html(content)
        else:
            return original_size, original_size, False
        
        # Guardar en directorio padre (fuera de /src) usando MISMO nombre
        # data/web/dashboard/src/dashboard.js → data/web/dashboard/dashboard.js
        # Así el HTML y C++ pueden referenciar el nombre original sin .min
        parent_dir = src_path.parent.parent
        out_path = parent_dir / src_path.name  # Mismo nombre, sin .min
        
        # Guardar archivo minificado en raíz del módulo web
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(minified_content)
        
        # Crear versión gzip para que AsyncWebServer la sirva automáticamente
        gz_path = Path(str(out_path) + '.gz')
        minified_bytes = minified_content.encode('utf-8')
        # mtime=0: salida determinista, el .gz solo cambia si cambia el contenido
        # (gzip.open guardaba la fecha y ensuciaba git en cada compilación)
        gz_path.write_bytes(gzip.compress(minified_bytes, compresslevel=9, mtime=0))
        gz_size = gz_path.stat().st_size
        
        final_size = len(minified_content)
        return original_size, final_size, True, str(out_path.relative_to(DATA_WEB_DIR)), gz_size
        
    except Exception as e:
        print(f"❌ Error procesando {src_path.name}: {e}")
        return 0, 0, False, "", 0

def minify_web_files():
    """
    Minifica archivos desde carpetas /src hacia directorio padre
    Estructura: data/web/{module}/src/*.{js,css,html} → data/web/{module}/*.min.{ext}
    """
    print("\n🔧 Iniciando minificación de archivos web...")
    print("📂 Buscando archivos en carpetas /src...\n")
    
    if not DATA_WEB_DIR.exists():
        print(f"⚠️  Directorio {DATA_WEB_DIR} no encontrado")
        return
    
    total_original = 0
    total_final = 0
    total_gz = 0
    processed_files = 0
    
    # Buscar carpetas /src dentro de data/web
    src_dirs = list(DATA_WEB_DIR.rglob("src"))
    
    if not src_dirs:
        print("⚠️  No se encontraron carpetas /src en data/web/")
        return
    
    for src_dir in src_dirs:
        module_name = src_dir.parent.name
        print(f"📦 Procesando módulo: {module_name}")
        
        # Procesar archivos JS, CSS, HTML en carpeta /src
        for file_path in src_dir.glob("*"):
            if file_path.suffix.lower() in ['.js', '.css', '.html']:
                orig_size, final_size, processed, output_path, gz_size = process_file(file_path)
                
                if processed:
                    total_original += orig_size
                    total_final += final_size
                    total_gz += gz_size
                    processed_files += 1
                    reduction = ((orig_size - final_size) / orig_size * 100) if orig_size > 0 else 0
                    gz_reduction = ((orig_size - gz_size) / orig_size * 100) if orig_size > 0 else 0
                    print(f"  ✓ {file_path.name} → {output_path}")
                    print(f"    {orig_size:,} → {final_size:,} bytes ({reduction:.1f}% reducción)")
                    print(f"    gzip: {gz_size:,} bytes ({gz_reduction:.1f}% reducción total)")
        
        print()  # Línea en blanco entre módulos
    
    if processed_files > 0:
        total_reduction = ((total_original - total_final) / total_original * 100) if total_original > 0 else 0
        gz_total_reduction = ((total_original - total_gz) / total_original * 100) if total_original > 0 else 0
        print(f"✅ Minificación completada:")
        print(f"   📊 Archivos procesados: {processed_files}")
        print(f"   📏 Tamaño original: {total_original:,} bytes ({total_original/1024:.1f} KB)")
        print(f"   📉 Tamaño minificado: {total_final:,} bytes ({total_final/1024:.1f} KB) [{total_reduction:.1f}%]")
        print(f"   🗜️ Tamaño gzip: {total_gz:,} bytes ({total_gz/1024:.1f} KB) [{gz_total_reduction:.1f}%]")
        print(f"   💾 Ahorro con gzip: ~{(total_original-total_gz)/1024:.1f} KB")
        print(f"\n   💡 Archivos fuente: data/web/*/src/")
        print(f"   💡 Archivos minificados: data/web/*/ (mismo nombre)")
        print(f"   💡 Archivos gzip: data/web/*/*.gz (servidos automáticamente)")
        print(f"   ✅ Listos para versionar en Git\n")
    else:
        print(f"⚠️  No se encontraron archivos para minificar en carpetas /src\n")

# Ejecutar minificación cuando se importa desde PlatformIO
minify_web_files()
