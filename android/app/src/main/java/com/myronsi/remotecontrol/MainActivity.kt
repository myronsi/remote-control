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
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedWriter
import java.net.InetSocketAddress
import java.net.Socket

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
        val currentWriter = writer ?: return
        currentWriter.write(json)
        currentWriter.newLine()
        currentWriter.flush()
    }

    @Synchronized
    fun close() {
        writer = null
        try {
            socket?.close()
        } catch (_: Exception) {
        }
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

    fun sendButton(button: String) {
        scope.launch(Dispatchers.IO) {
            try {
                client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"$button\",\"action\":\"down\"}")
                client.send("{\"version\":1,\"type\":\"mouse_button\",\"button\":\"$button\",\"action\":\"up\"}")
            } catch (_: Exception) {
                withContext(Dispatchers.Main) {
                    connected = false
                    status = "Connection lost"
                }
                client.close()
            }
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
                            connectTo(device.host, device.port, device.name)
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
                    connectTo(manualHost.trim(), 48721, manualHost.trim())
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
                        detectDragGestures(
                            onDrag = { change, dragAmount ->
                                change.consume()

                                val dx = dragAmount.x.toInt()
                                val dy = dragAmount.y.toInt()

                                if (dx != 0 || dy != 0) {
                                    scope.launch(Dispatchers.IO) {
                                        try {
                                            client.send(
                                                "{\"version\":1,\"type\":\"mouse_move\",\"dx\":$dx,\"dy\":$dy}"
                                            )
                                        } catch (_: Exception) {
                                            withContext(Dispatchers.Main) {
                                                connected = false
                                                status = "Connection lost"
                                            }
                                            client.close()
                                        }
                                    }
                                }
                            }
                        )
                    },
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally
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
                    onClick = { sendButton("left") }
                ) {
                    Text("Left")
                }

                Button(
                    modifier = Modifier.weight(1f),
                    onClick = { sendButton("right") }
                ) {
                    Text("Right")
                }
            }
        }
    }
}
