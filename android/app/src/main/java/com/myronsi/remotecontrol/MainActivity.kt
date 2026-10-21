package com.myronsi.remotecontrol

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.PointerEvent
import androidx.compose.ui.input.pointer.PointerEventType
import androidx.compose.ui.input.pointer.awaitPointerEvent
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedWriter
import java.net.InetSocketAddress
import java.net.Socket
import kotlin.math.abs

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                RemoteControlScreen()
            }
        }
    }
}

private class RemoteClient {
    private var socket: Socket? = null
    private var writer: BufferedWriter? = null

    @Synchronized
    fun connect(host: String, port: Int) {
        close()
        val newSocket = Socket()
        newSocket.tcpNoDelay = true
        newSocket.keepAlive = true
        newSocket.connect(InetSocketAddress(host, port), 3000)
        socket = newSocket
        writer = newSocket.getOutputStream().bufferedWriter(Charsets.UTF_8)
        send("{\"version\":1,\"type\":\"hello\"}")
    }

    @Synchronized
    fun send(json: String) {
        val currentWriter = writer ?: throw IllegalStateException("Not connected")
        currentWriter.write(json)
        currentWriter.newLine()
        currentWriter.flush()
    }

    @Synchronized
    fun close() {
        writer = null
        try { socket?.close() } catch (_: Exception) { }
        socket = null
    }
}

@Composable
private fun RemoteControlScreen() {
    val scope = rememberCoroutineScope()
    val context = androidx.compose.ui.platform.LocalContext.current
    val client = remember { RemoteClient() }
    val discovery = remember { PcDiscovery(context.applicationContext) }
    val devices by discovery.devices.collectAsState()

    var status by remember { mutableStateOf("Searching for PCs...") }
    var connected by remember { mutableStateOf(false) }
    var manualHost by remember { mutableStateOf("") }

    DisposableEffect(discovery) {
        discovery.start()
        onDispose {
            discovery.stop()
            client.close()
        }
    }

    fun connectTo(host: String, port: Int, name: String) {
        status = "Connecting to $name..."
        scope.launch(Dispatchers.IO) {
            try {
                client.connect(host, port)
                withContext(Dispatchers.Main) {
                    connected = true
                    status = "Connected to $name"
                }
            } catch (e: Exception) {
                client.close()
                withContext(Dispatchers.Main) {
                    connected = false
                    status = "Connection failed: ${e.message ?: "unknown error"}"
                }
            }
        }
    }

    fun sendCommand(json: String) {
        scope.launch(Dispatchers.IO) {
            try {
                client.send(json)
            } catch (_: Exception) {
                client.close()
                withContext(Dispatchers.Main) {
                    connected = false
                    status = "Connection lost"
                }
            }
        }
    }

    fun sendButton(button: String) {
        sendCommand("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"$button\",\"action\":\"down\"}")
        sendCommand("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"$button\",\"action\":\"up\"}")
    }

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Remote Control", style = MaterialTheme.typography.headlineMedium)
        Text(status)

        if (!connected) {
            Text("Available PCs", style = MaterialTheme.typography.titleMedium)
            if (devices.isEmpty()) {
                Text("No PCs found yet.")
            } else {
                devices.forEach { device ->
                    Button(
                        modifier = Modifier.fillMaxWidth(),
                        onClick = { connectTo(device.host, device.port, device.name) }
                    ) {
                        Column {
                            Text(device.name)
                            Text("${device.host}:${device.port}")
                        }
                    }
                }
            }

            OutlinedTextField(
                value = manualHost,
                onValueChange = { manualHost = it },
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Manual IP (development fallback)") },
                singleLine = true
            )

            Button(
                modifier = Modifier.fillMaxWidth(),
                enabled = manualHost.isNotBlank(),
                onClick = { connectTo(manualHost.trim(), 48721, manualHost.trim()) }
            ) {
                Text("Connect manually")
            }
        } else {
            Button(
                modifier = Modifier.fillMaxWidth(),
                onClick = {
                    client.close()
                    connected = false
                    status = "Disconnected"
                }
            ) { Text("Disconnect") }

            Spacer(Modifier.height(4.dp))

            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color.LightGray)
                    .pointerInput(connected) {
                        awaitPointerEventScope {
                            while (true) {
                                val down = awaitFirstDown(requireUnconsumed = false)
                                val startTime = System.currentTimeMillis()
                                var totalX = 0f
                                var totalY = 0f
                                var rightClickSent = false

                                while (true) {
                                    val event: PointerEvent = awaitPointerEvent()
                                    val pressed = event.changes.filter { it.pressed }
                                    val pointerCount = pressed.size

                                    if (pointerCount == 0) {
                                        val elapsed = System.currentTimeMillis() - startTime
                                        if (!rightClickSent && abs(totalX) < 20f && abs(totalY) < 20f) {
                                            if (elapsed >= 500L) {
                                                sendButton("right")
                                            } else {
                                                sendButton("left")
                                            }
                                        }
                                        break
                                    }

                                    if (pointerCount >= 2) {
                                        val dy = pressed.sumOf { it.positionChange().y.toDouble() }.toFloat() / pointerCount
                                        val dx = pressed.sumOf { it.positionChange().x.toDouble() }.toFloat() / pointerCount
                                        if (abs(dy) >= 1f || abs(dx) >= 1f) {
                                            sendCommand("{\"version\":1,\"type\":\"mouse_scroll\",\"dx\":${(-dx / 12f).toInt()},\"dy\":${(-dy / 12f).toInt()}}")
                                        }
                                    } else {
                                        val change = pressed.first()
                                        val delta = change.positionChange()
                                        totalX += delta.x
                                        totalY += delta.y

                                        if (!rightClickSent && System.currentTimeMillis() - startTime >= 500L && abs(totalX) < 20f && abs(totalY) < 20f) {
                                            sendButton("right")
                                            rightClickSent = true
                                        }

                                        if (delta.x != 0f || delta.y != 0f) {
                                            val dx = delta.x.toInt()
                                            val dy = delta.y.toInt()
                                            if (dx != 0 || dy != 0) {
                                                sendCommand("{\"version\":1,\"type\":\"mouse_move\",\"dx\":$dx,\"dy\":$dy}")
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    },
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text("Touchpad", style = MaterialTheme.typography.headlineSmall)
                Text("Drag • tap • two-finger scroll")
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(modifier = Modifier.weight(1f), onClick = { sendButton("left") }) {
                    Text("Left")
                }
                Button(modifier = Modifier.weight(1f), onClick = { sendButton("right") }) {
                    Text("Right")
                }
            }
        }
    }
}
