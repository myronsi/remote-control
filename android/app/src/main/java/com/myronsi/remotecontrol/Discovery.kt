package com.myronsi.remotecontrol

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class DiscoveredPc(
    val name: String,
    val host: String,
    val port: Int
)

class PcDiscovery(context: Context) {
    companion object {
        const val SERVICE_TYPE = "_remotecontrol._tcp."
    }

    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val _devices = MutableStateFlow<List<DiscoveredPc>>(emptyList())
    val devices: StateFlow<List<DiscoveredPc>> = _devices

    private var listener: NsdManager.DiscoveryListener? = null

    fun start() {
        if (listener != null) return

        val discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) = Unit

            override fun onServiceFound(serviceInfo: NsdServiceInfo) {
                if (serviceInfo.serviceType != SERVICE_TYPE) return

                nsdManager.resolveService(
                    serviceInfo,
                    object : NsdManager.ResolveListener {
                        override fun onResolveFailed(info: NsdServiceInfo, errorCode: Int) = Unit

                        override fun onServiceResolved(info: NsdServiceInfo) {
                            val host = info.host?.hostAddress ?: return
                            val pc = DiscoveredPc(
                                name = info.serviceName,
                                host = host,
                                port = info.port
                            )

                            _devices.value = _devices.value
                                .filterNot { it.name == pc.name }
                                .plus(pc)
                                .sortedBy { it.name.lowercase() }
                        }
                    }
                )
            }

            override fun onServiceLost(serviceInfo: NsdServiceInfo) {
                _devices.value = _devices.value.filterNot { it.name == serviceInfo.serviceName }
            }

            override fun onDiscoveryStopped(serviceType: String) = Unit
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                stop()
            }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
                listener = null
            }
        }

        listener = discoveryListener
        nsdManager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
    }

    fun stop() {
        val current = listener ?: return
        runCatching { nsdManager.stopServiceDiscovery(current) }
        listener = null
    }
}
