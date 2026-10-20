# 📁 Estructura de Archivos Web

Este proyecto utiliza una estructura organizada para separar código fuente de archivos de producción.

## 📂 Organización

```
data/web/
├── captive_portal/
│   ├── src/                    ← 📝 EDITA AQUÍ (código fuente)
│   │   ├── captive-portal.js
│   │   ├── index.html
│   │   └── style.css
│   ├── captive-portal.min.js   ← 🔒 AUTO-GENERADO (minificado)
│   ├── index.min.html          ← 🔒 AUTO-GENERADO (minificado)
│   └── style.min.css           ← 🔒 AUTO-GENERADO (minificado)
│
├── dashboard/
│   ├── src/                    ← 📝 EDITA AQUÍ
│   │   ├── dashboard.js
│   │   ├── index.html
│   │   └── style.css
│   └── *.min.*                 ← 🔒 AUTO-GENERADO
│
├── espnow_dashboard/
│   ├── src/                    ← 📝 EDITA AQUÍ
│   └── *.min.*                 ← 🔒 AUTO-GENERADO
│
└── esp_now/
    ├── src/                    ← 📝 EDITA AQUÍ
    └── *.min.*                 ← 🔒 AUTO-GENERADO
```

## 🎯 Workflow de Desarrollo

### 1️⃣ Editar Código Fuente
Trabaja siempre en las carpetas `/src`:
```
data/web/{module}/src/archivo.js    ← Edita aquí
```

### 2️⃣ Minificación Automática
Al compilar con PlatformIO, el script genera automáticamente versiones minificadas:
```bash
pio run                  # Compila firmware
pio run --target buildfs # Compila filesystem + minificación
```

El script `scripts/minify_web.py` procesa:
- `data/web/*/src/*.js`   → `data/web/*/*.min.js`
- `data/web/*/src/*.css`  → `data/web/*/*.min.css`
- `data/web/*/src/*.html` → `data/web/*/*.min.html`

### 3️⃣ Archivos que se Despliegan al ESP32
El ESP32 sirve los archivos **minificados** desde:
```
data/web/{module}/*.min.*
```

## 📊 Beneficios

✅ **Código legible**: Archivos originales siempre disponibles en `/src`  
✅ **Optimización**: Archivos minificados reducen 28.2% del tamaño (~80KB)  
✅ **Git limpio**: Ambos versionados para transparencia total  
✅ **Sin configuración extra**: ESP32 no requiere cambios en código C++  
✅ **Build automático**: Minificación integrada en proceso de compilación  

## 🔧 Minificación Manual

Si necesitas regenerar archivos minificados sin compilar:
```bash
python scripts/minify_web.py
```

## 📝 Convenciones

- **Archivos fuente**: `data/web/*/src/`  
  - Versionados en Git ✅
  - Código legible, comentado
  - Para desarrollo y mantenimiento

- **Archivos minificados**: `data/web/*/*.min.*`  
  - Versionados en Git ✅
  - Código comprimido, sin espacios
  - Para producción en ESP32

## ⚠️ Importante

**NO edites archivos `.min.*` directamente**  
Los cambios se perderán en la próxima compilación.  
Siempre edita los archivos en carpetas `/src`.
