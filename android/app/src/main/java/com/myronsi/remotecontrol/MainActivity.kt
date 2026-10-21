package com.myronsi.remotecontrol

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.weight
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.runtime.collectAsState
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.consume
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

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
    private var socket: java.net.Socket? = null
    private var writer: java.io.BufferedWriter? = null

    @Synchronized
    fun connect(host: String, port: Int) {
        socket?.close()
        val newSocket = java.net.Socket(host, port)
        newSocket.tcpNoDelay = true
        socket = newSocket
        writer = newSocket.getOutputStream().bufferedWriter(Charsets.UTF_8)
        send("{\"version\":1,\"type\":\"hello\"}")
    }

    @Synchronized
    fun send(json: String) {
        val currentWriter = writer ?: return
        currentWriter.write(json)
        currentWriter.newLine()
        currentWriter.flush()
    }

    @Synchronized
    fun close() {
        writer = null
        socket?.close()
        socket = null
    }
}

@androidx.compose.runtime.Composable
private fun RemoteControlScreen() {
    val scope = rememberCoroutineScope()
    val context = androidx.compose.ui.platform.LocalContext.current
    val client = remember { RemoteClient() }
    val discovery = remember { PcDiscovery(context.applicationContext) }
    val devices by discovery.devices.collectAsState()

    var status by remember { mutableStateOf("Searching for PCs...") }
    var connected by remember { mutableStateOf(false) }
    var selectedDevice by remember { mutableStateOf<DiscoveredPc?>(null) }
    var manualHost by remember { mutableStateOf("") }

    DisposableEffect(discovery) {
        discovery.start()
        onDispose {
            discovery.stop()
            client.close()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
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
                        onClick = {
                            selectedDevice = device
                            status = "Connecting to ${device.name}..."
                            scope.launch(Dispatchers.IO) {
                                try {
                                    client.connect(device.host, device.port)
                                    withContext(Dispatchers.Main) {
                                        connected = true
                                        status = "Connected to ${device.name}"
                                    }
                                } catch (e: Exception) {
                                    withContext(Dispatchers.Main) {
                                        connected = false
                                        status = "Connection failed: ${e.message ?: "unknown error"}"
                                    }
                                }
                            }
                        }
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
                onClick = {
                    val host = manualHost.trim()
                    status = "Connecting..."
                    scope.launch(Dispatchers.IO) {
                        try {
                            client.connect(host, 48721)
                            withContext(Dispatchers.Main) {
                                connected = true
                                status = "Connected to $host"
                            }
                        } catch (e: Exception) {
                            withContext(Dispatchers.Main) {
                                status = "Connection failed: ${e.message ?: "unknown error"}"
                            }
                        }
                    }
                }
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
            ) {
                Text("Disconnect")
            }

            Spacer(Modifier.height(4.dp))

            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .background(Color.LightGray)
                    .pointerInput(connected) {
                        detectDragGestures { change, dragAmount ->
                            change.consume()
                            val dx = dragAmount.x.toInt()
                            val dy = dragAmount.y.toInt()
                            if (dx != 0 || dy != 0) {
                                scope.launch(Dispatchers.IO) {
                                    client.send(
                                        "{\"version\":1,\"type\":\"mouse_move\",\"dx\":$dx,\"dy\":$dy}"
                                    )
                                }
                            }
                        }
                    },
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = androidx.compose.ui.Alignment.CenterHorizontally
            ) {
                Text("Touchpad", style = MaterialTheme.typography.headlineSmall)
                Text("Drag to move the PC cursor")
            }

            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(
                    modifier = Modifier.weight(1f),
                    onClick = {
                        scope.launch(Dispatchers.IO) {
                            client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"left\",\"action\":\"down\"}")
                            client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"left\",\"action\":\"up\"}")
                        }
                    }
                ) { Text("Left") }

                Button(
                    modifier = Modifier.weight(1f),
                    onClick = {
                        scope.launch(Dispatchers.IO) {
                            client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"right\",\"action\":\"down\"}")
                            client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"right\",\"action\":\"up\"}")
                        }
                    }
                ) { Text("Right") }
            }
        }
    }
}
