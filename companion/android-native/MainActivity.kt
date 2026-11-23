package io.wristfoundry.wristfoundry_companion

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.pm.PackageManager
import android.os.*
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel
import java.io.ByteArrayOutputStream
import java.util.UUID
import java.util.ArrayDeque

/** Foreground-only, service-filtered BLE bridge. No credentials or pairing codes logged. */
@SuppressLint("MissingPermission")
class MainActivity : FlutterActivity() {
    private val serviceId = UUID.fromString("8c8e0001-6e9b-4b71-9e1d-42b47c574631")
    private val rxId = UUID.fromString("8c8e0002-6e9b-4b71-9e1d-42b47c574631")
    private val txId = UUID.fromString("8c8e0003-6e9b-4b71-9e1d-42b47c574631")
    private val cccdId = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val handler = Handler(Looper.getMainLooper())
    private val adapter get() = (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var opening: MethodChannel.Result? = null
    private var permissionResult: MethodChannel.Result? = null
    private var mtu = 23
    private var scanning = false
    private var ready = false
    private val found = linkedMapOf<String, Map<String, Any>>()
    private val input = ByteArrayOutputStream()
    private data class Write(val data: ByteArray, val result: MethodChannel.Result, var offset: Int = 0)
    private val writes = ArrayDeque<Write>()
    private val openTimeout = Runnable { fail("pairing_failed") }
    private val writeTimeout = Runnable { fail("write_failed") }
    private val scanStop = Runnable { stopScan() }
    private var lastScan = 0L

    override fun configureFlutterEngine(engine: FlutterEngine) {
        super.configureFlutterEngine(engine)
        MethodChannel(engine.dartExecutor.binaryMessenger, "io.wristfoundry/ble").setMethodCallHandler { call, result ->
            try {
                when (call.method) {
                    "enumerate" -> enumerate(result)
                    "open" -> {
                        if (opening != null || gatt != null) { result.error("busy", null, null); return@setMethodCallHandler }
                        val address = call.argument<String>("port")
                        if (address == null || !found.containsKey(address)) { result.error("wrong_device", null, null); return@setMethodCallHandler }
                        stopScan(); input.reset(); mtu = 23; ready = false
                        opening = result
                        handler.postDelayed(openTimeout, 45000)
                        gatt = adapter.getRemoteDevice(address).connectGatt(this, false, callback, BluetoothDevice.TRANSPORT_LE)
                    }
                    "read" -> {
                        if (!ready) result.error("disconnected", null, null)
                        else { val data = input.toByteArray(); input.reset(); result.success(data); data.fill(0) }
                    }
                    "write" -> {
                        val data = call.arguments as? ByteArray
                        if (!ready || data == null) result.error("disconnected", null, null)
                        else if (data.size > 1025 || writes.size >= 4) result.error("busy", null, null)
                        else { writes.add(Write(data.copyOf(), result)); if (writes.size == 1) sendChunk() }
                    }
                    "close" -> { close(); result.success(null) }
                    else -> result.notImplemented()
                }
            } catch (_: SecurityException) {
                val pending = opening === result || writes.any { it.result === result }
                fail("permission_denied")
                if (!pending) result.error("permission_denied", null, null)
            } catch (_: Exception) {
                val pending = opening === result || writes.any { it.result === result }
                fail("disconnected")
                if (!pending) result.error("disconnected", null, null)
            }
        }
    }
    private fun permissions(): Array<String> = if (Build.VERSION.SDK_INT >= 31)
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    private fun enumerate(result: MethodChannel.Result) {
        val missing = permissions().filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) {
            if (permissionResult != null) { result.error("busy", null, null); return }
            permissionResult = result; requestPermissions(missing.toTypedArray(), 41); return
        }
        if (adapter == null || !adapter.isEnabled) { result.error("bluetooth_off", null, null); return }
        if (!ready && !scanning && SystemClock.elapsedRealtime() - lastScan >= 15000) {
            found.clear(); lastScan = SystemClock.elapsedRealtime(); scanning = true
            adapter.bluetoothLeScanner.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(serviceId)).build()),
                ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback)
            handler.postDelayed(scanStop, 8000)
        }
        result.success(found.values.toList())
    }
    override fun onRequestPermissionsResult(code: Int, permissions: Array<out String>, grants: IntArray) {
        super.onRequestPermissionsResult(code, permissions, grants)
        if (code == 41) {
            val result = permissionResult; permissionResult = null
            if (grants.isNotEmpty() && grants.all { it == PackageManager.PERMISSION_GRANTED }) {
                if (result != null) enumerate(result)
            } else result?.error("permission_denied", null, null)
        }
    }
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(type: Int, value: ScanResult) {
            handler.post {
                if (scanning && found.size < 24) found[value.device.address] = mapOf(
                    "port" to value.device.address, "label" to (value.scanRecord?.deviceName ?: "WristFoundry"))
            }
        }
        override fun onScanFailed(errorCode: Int) { handler.post { scanning = false } }
    }
    private fun stopScan() {
        handler.removeCallbacks(scanStop)
        if (scanning) { try { adapter?.bluetoothLeScanner?.stopScan(scanCallback) } catch (_: SecurityException) {} }
        scanning = false
    }
    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(client: BluetoothGatt, status: Int, newState: Int) {
            handler.post {
                if (client !== gatt) return@post
                if (status != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) { fail("disconnected"); return@post }
                if (newState == BluetoothProfile.STATE_CONNECTED && !client.requestMtu(185)) fail("disconnected")
            }
        }
        override fun onMtuChanged(client: BluetoothGatt, value: Int, status: Int) {
            handler.post {
                if (client !== gatt) return@post
                if (status == BluetoothGatt.GATT_SUCCESS) mtu = value.coerceIn(23, 247)
                if (!client.discoverServices()) fail("disconnected")
            }
        }
        override fun onServicesDiscovered(client: BluetoothGatt, status: Int) {
            handler.post {
                if (client !== gatt) return@post
                val service = client.getService(serviceId)
                rx = service?.getCharacteristic(rxId)
                val tx = service?.getCharacteristic(txId)
                val descriptor = tx?.getDescriptor(cccdId)
                if (status != BluetoothGatt.GATT_SUCCESS || rx == null || tx == null || descriptor == null || !client.setCharacteristicNotification(tx, true)) {
                    fail("incompatible_firmware"); return@post
                }
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                if (!client.writeDescriptor(descriptor)) fail("disconnected")
            }
        }
        override fun onDescriptorWrite(client: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            handler.post {
                if (client !== gatt) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { fail("pairing_failed"); return@post }
                // Encrypted/authenticated RX triggers Android's system passkey dialog.
                // RS alone is not a command and is overwritten by the hello frame.
                if (!writeValue(byteArrayOf(30))) fail("pairing_failed")
            }
        }
        override fun onCharacteristicWrite(client: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            handler.post {
                if (client !== gatt) return@post
                if (status != BluetoothGatt.GATT_SUCCESS) { fail("pairing_failed"); return@post }
                if (opening != null) {
                    handler.removeCallbacks(openTimeout); ready = true
                    val result = opening; opening = null; result?.success(null)
                } else {
                    handler.removeCallbacks(writeTimeout)
                    val item = writes.peek() ?: return@post
                    item.offset += minOf(mtu - 3, item.data.size - item.offset)
                    if (item.offset >= item.data.size) { writes.remove(); item.data.fill(0); item.result.success(null) }
                    sendChunk()
                }
            }
        }
        @Deprecated("Legacy callback for Android before API 33")
        override fun onCharacteristicChanged(client: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            receive(client, characteristic.value?.copyOf() ?: return)
        }
        override fun onCharacteristicChanged(client: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) { receive(client, value.copyOf()) }
    }
    private fun receive(client: BluetoothGatt, data: ByteArray) {
        handler.post {
            if (client === gatt && ready) {
                if (input.size() + data.size > 32768) fail("frame_too_large") else input.write(data)
            }
            data.fill(0)
        }
    }
    private fun writeValue(data: ByteArray): Boolean {
        val client = gatt ?: return false
        val characteristic = rx ?: return false
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        @Suppress("DEPRECATION")
        characteristic.value = data
        @Suppress("DEPRECATION")
        return client.writeCharacteristic(characteristic)
    }
    private fun sendChunk() {
        val item = writes.peek() ?: return
        val chunk = item.data.copyOfRange(item.offset, minOf(item.offset + mtu - 3, item.data.size))
        if (!writeValue(chunk)) { fail("write_failed"); return }
        handler.postDelayed(writeTimeout, 4500)
    }
    private fun fail(code: String) {
        val result = opening; opening = null; result?.error(code, null, null)
        while (writes.isNotEmpty()) { val item = writes.remove(); item.data.fill(0); item.result.error(code, null, null) }
        close()
    }
    private fun close() {
        stopScan(); handler.removeCallbacks(openTimeout); handler.removeCallbacks(writeTimeout)
        val result = opening; opening = null; result?.error("disconnected", null, null)
        while (writes.isNotEmpty()) { val item = writes.remove(); item.data.fill(0); item.result.error("disconnected", null, null) }
        val client = gatt; gatt = null; ready = false; rx = null
        try { client?.disconnect(); client?.close() } catch (_: SecurityException) {}
        input.reset()
    }
    override fun onStop() {
        super.onStop(); stopScan()
        if (opening == null) close() // Do not cancel the Android pairing dialog.
    }
    override fun onDestroy() { close(); super.onDestroy() }
}
