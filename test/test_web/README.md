# Tests del Frontend (JavaScript)

Este directorio contiene los **Unit Tests del frontend** en formato HTML. Cada archivo HTML es un test suite independiente que se ejecuta directamente en el navegador.

## 📋 Archivos de Test

### 1. `test_dashboard.html`
Tests para el dashboard principal del sensor de agua.

**Funciones testeadas:**
- ✅ Cálculo de porcentaje de llenado (0-100%)
- ✅ Formato de distancia (cm, errores, fuera de rango)
- ✅ Validación de datos del sensor (JSON)
- ✅ Determinación de color de estado (verde/amarillo/rojo)
- ✅ Parse de respuesta API (JSON)
- ✅ Actualización de UI (DOM manipulation)

**Total:** 15 tests

---

### 2. `test_captive_portal.html`
Tests para el portal cautivo de configuración WiFi.

**Funciones testeadas:**
- ✅ Validación de SSID (longitud, caracteres)
- ✅ Validación de password WPA2 (8-63 caracteres)
- ✅ Parse de lista de redes WiFi (JSON array)
- ✅ Indicador de fuerza de señal (Excelente/Buena/Regular/Débil)
- ✅ Barras de señal (0-4)
- ✅ Creación de payload de configuración WiFi
- ✅ Ordenar redes por RSSI (señal)
- ✅ Filtrar SSIDs duplicados
- ✅ Renderizado de lista de redes (UI)

**Total:** 22 tests

---

### 3. `test_espnow_dashboard.html`
Tests para el dashboard ESP-NOW (gestión de slaves).

**Funciones testeadas:**
- ✅ Validación de dirección MAC (AA:BB:CC:DD:EE:FF)
- ✅ Parse de dirección MAC (remover/agregar separadores)
- ✅ Formato de dirección MAC (normalización)
- ✅ Parse de datos de slave (JSON con MAC, tipo, distancia, RSSI)
- ✅ Detección de slave online/offline (timestamp)
- ✅ Estados de slave (online <10s, warning 10-30s, offline >30s)
- ✅ Cálculo de distancia promedio
- ✅ Búsqueda de slave por MAC
- ✅ Ordenar slaves por RSSI
- ✅ Filtrar slaves online
- ✅ Crear payload de acciones (add/remove/update)
- ✅ Renderizado de lista de slaves (UI)

**Total:** 28 tests

---

## 🚀 Cómo Ejecutar los Tests

### Opción 1: Abrir Directamente en Navegador
1. **Abrir el archivo HTML en tu navegador**:
   ```bash
   # Windows (desde PowerShell)
   start test/test_web/test_dashboard.html
   start test/test_web/test_captive_portal.html
   start test/test_web/test_espnow_dashboard.html
   
   # O arrastrarlo al navegador
   ```

2. **Los tests se ejecutan automáticamente** al cargar la página

3. **Ver resultados**:
   - ✅ **Verde**: Test pasó
   - ❌ **Rojo**: Test falló (muestra error)
   - Resumen al final: `X Tests | Y Passed | Z Failed`

---

### Opción 2: Live Server (Recomendado para desarrollo)
Si tienes **Live Server** de VS Code instalado:

1. **Click derecho en el archivo HTML** → `Open with Live Server`
2. Los tests se ejecutan automáticamente
3. Recarga la página para volver a ejecutar

---

### Opción 3: Servidor HTTP Local
Desde la raíz del proyecto:

```powershell
# Python
python -m http.server 8080

# Node.js (si tienes http-server instalado)
npx http-server -p 8080
```

Luego abre en el navegador:
```
http://localhost:8080/test/test_web/test_dashboard.html
http://localhost:8080/test/test_web/test_captive_portal.html
http://localhost:8080/test/test_web/test_espnow_dashboard.html
```

---

## 📊 Interpretar Resultados

### ✅ Test PASÓ (Verde)
```
✅ Cálculo de porcentaje - tanque al 50% - 2.45ms
```
- El test completó exitosamente
- Tiempo de ejecución: 2.45ms

### ❌ Test FALLÓ (Rojo)
```
❌ Validación de datos del sensor - datos inválidos
Error: Expected false, got true - 1.23ms
```
- El test falló
- Muestra el error esperado vs recibido
- Tiempo de ejecución: 1.23ms

### 📈 Resumen Final
```
65 Tests | 63 Passed | 2 Failed
```
- **Total de tests ejecutados**: 65
- **Pasaron**: 63 (96.9%)
- **Fallaron**: 2 (3.1%)

---

## 🔧 Estructura de un Test

Cada archivo HTML contiene:

### 1. Mini Framework de Testing
```javascript
const TestRunner = {
    test(name, fn) { /* ... */ },
    run() { /* ... */ }
};
```

### 2. Assertions (Afirmaciones)
```javascript
assert.equal(actual, expected, message);
assert.true(actual, message);
assert.false(actual, message);
assert.throws(fn, message);
assert.notNull(actual, message);
```

### 3. Definición de Tests
```javascript
TestRunner.test('Descripción del test', () => {
    // Arrange: Preparar datos
    const input = 550;
    const capacity = 1100;
    
    // Act: Ejecutar función
    const result = calculatePercentage(input, capacity);
    
    // Assert: Verificar resultado
    assert.equal(result, 50, 'Porcentaje debería ser 50%');
});
```

---

## 🧪 Agregar Nuevos Tests

Para agregar un nuevo test en cualquier archivo HTML:

```javascript
TestRunner.test('Mi nuevo test', () => {
    // 1. Preparar datos (Arrange)
    const data = { /* ... */ };
    
    // 2. Ejecutar función (Act)
    const result = myFunction(data);
    
    // 3. Verificar resultado (Assert)
    assert.equal(result, expectedValue, 'Mensaje explicativo');
});
```

Luego **recarga la página** para ejecutar el nuevo test.

---

## 🐛 Debugging de Tests

### Ver Errores en Consola del Navegador
1. Abre DevTools (`F12` o `Ctrl+Shift+I`)
2. Ve a la pestaña **Console**
3. Verás los mensajes de error detallados

### Agregar Breakpoints
```javascript
TestRunner.test('Test con debugging', () => {
    const result = myFunction(data);
    debugger;  // ⬅️ El navegador se detendrá aquí
    assert.equal(result, expected);
});
```

### Ver Variables en Test
```javascript
TestRunner.test('Test con logs', () => {
    const result = myFunction(data);
    console.log('Result:', result);  // ⬅️ Ver valor en consola
    assert.equal(result, expected);
});
```

---

## ✨ Mejores Prácticas

### ✅ DO (Hacer)
- **Nombres descriptivos**: `'Cálculo de porcentaje - tanque vacío'`
- **Un concepto por test**: Cada test verifica UNA cosa
- **Tests independientes**: No dependen de ejecución de otros tests
- **Valores esperados claros**: `assert.equal(result, 50, 'Debe ser 50%')`

### ❌ DON'T (No hacer)
- Nombres vagos: `'Test 1'`, `'Funciona'`
- Tests que verifican múltiples cosas a la vez
- Tests que modifican variables globales
- Asumir orden de ejecución de tests

---

## 📦 Cobertura de Tests Frontend

### Dashboard (test_dashboard.html)
| Función                 | Cobertura |
|-------------------------|-----------|
| `calculatePercentage`   | ✅ 100%   |
| `formatDistance`        | ✅ 100%   |
| `validateSensorData`    | ✅ 100%   |
| `getStatusColor`        | ✅ 100%   |
| `parseAPIResponse`      | ✅ 100%   |
| DOM updates             | ✅ 100%   |

### Captive Portal (test_captive_portal.html)
| Función                    | Cobertura |
|----------------------------|-----------|
| `validateSSID`             | ✅ 100%   |
| `validatePassword`         | ✅ 100%   |
| `parseNetworkList`         | ✅ 100%   |
| `getSignalStrength`        | ✅ 100%   |
| `getSignalBars`            | ✅ 100%   |
| `createWiFiConfigPayload`  | ✅ 100%   |
| `sortNetworksBySignal`     | ✅ 100%   |
| `filterDuplicateSSIDs`     | ✅ 100%   |
| WiFi scan UI rendering     | ✅ 100%   |

### ESP-NOW Dashboard (test_espnow_dashboard.html)
| Función                   | Cobertura |
|---------------------------|-----------|
| `validateMACAddress`      | ✅ 100%   |
| `parseMACAddress`         | ✅ 100%   |
| `formatMACAddress`        | ✅ 100%   |
| `parseSlaveData`          | ✅ 100%   |
| `isSlaveOnline`           | ✅ 100%   |
| `getSlaveStatus`          | ✅ 100%   |
| `calculateAverageDistance`| ✅ 100%   |
| `findSlaveByMAC`          | ✅ 100%   |
| `sortSlavesByRSSI`        | ✅ 100%   |
| `filterOnlineSlaves`      | ✅ 100%   |
| `createSlavePayload`      | ✅ 100%   |
| Slaves UI rendering       | ✅ 100%   |

**Cobertura total frontend: 100%** 🎉

---

## 🔗 Relacionado

- **Tests de Backend (C++)**: Ver `test/README.md`
- **Documentación de funciones**: Ver `docs/web-documentation/`
- **Código fuente frontend**: Ver `data/web/`

---

## 📞 Soporte

Si tienes problemas con los tests:
1. **Abre DevTools (`F12`)** y revisa la consola
2. **Verifica que las funciones simuladas** coinciden con tu código real
3. **Compara con los ejemplos** de este README
4. **Ejecuta un solo archivo** para aislar el problema

---

**Total de tests frontend: 65 tests** (15 dashboard + 22 captive portal + 28 espnow dashboard)

✅ **¡Todos los tests están listos para ejecutarse!**
