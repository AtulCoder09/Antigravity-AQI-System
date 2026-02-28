"use client";

import React, { useEffect, useState, useRef } from 'react';
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card';
import { Button } from '@/components/ui/button';
import { Badge } from '@/components/ui/badge';
import { Fan, Wind, AlertTriangle, CheckCircle, Activity, Settings2, Thermometer, Droplets, Gauge } from 'lucide-react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';

export default function Dashboard() {
    const [isConnected, setIsConnected] = useState(false);
    const [sensorData, setSensorData] = useState({
        mq135: 0,
        mq8: 0,
        mq9: 0,
        dust: 0,
    });
    const [mlStatus, setMlStatus] = useState({
        anomaly: false,
        status: "Waiting for data...",
        risk_score: 0,
        recommended_fan_speed: 0,
        total_aqi: 0,
        aqi_category: "—",
        temperature: 0,
        humidity: 0
    });

    const [history, setHistory] = useState<Array<{ time: string, risk: number, aqi: number, mq135: number, mq8: number, mq9: number, dust: number }>>([]);
    const ws = useRef<WebSocket | null>(null);

    useEffect(() => {
        // Connect to FastAPI WebSocket
        ws.current = new WebSocket("wss://antigravity-aqi-backend.onrender.com/ws/dashboard");

        ws.current.onopen = () => {
            console.log("Connected to AQI Intelligence Layer");
            setIsConnected(true);
        };

        ws.current.onclose = () => {
            console.log("Disconnected from AQI Intelligence Layer");
            setIsConnected(false);
        };

        ws.current.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === "telemetry") {
                    setSensorData(data.sensors);
                    setMlStatus(data.ml_insights);

                    setHistory(prev => {
                        const newHistory = [...prev, {
                            time: new Date().toLocaleTimeString(),
                            risk: data.ml_insights.risk_score,
                            aqi: data.ml_insights.total_aqi,
                            ...data.sensors
                        }];
                        // Keep last 20 data points
                        if (newHistory.length > 20) return newHistory.slice(1);
                        return newHistory;
                    });
                }
            } catch (err) {
                console.error("Failed to parse WebSocket message", err);
            }
        };

        return () => {
            ws.current?.close();
        };
    }, []);

    const handleCalibrate = () => {
        if (ws.current && isConnected) {
            ws.current.send(JSON.stringify({ command: "calibrate" }));
            alert("Calibration Command Sent to Edge Device!");
        } else {
            alert("Not connected to backend.");
        }
    };

    const handleManualFan = (speed: number) => {
        if (ws.current && isConnected) {
            ws.current.send(JSON.stringify({ command: "manual_fan", speed }));
        }
    };

    // AQI color coding based on EPA standard ranges
    const getAqiColor = (aqi: number) => {
        if (aqi <= 50) return 'text-emerald-400';
        if (aqi <= 100) return 'text-yellow-400';
        if (aqi <= 150) return 'text-orange-400';
        if (aqi <= 200) return 'text-red-400';
        if (aqi <= 300) return 'text-purple-400';
        return 'text-rose-500';
    };

    const getAqiBgColor = (aqi: number) => {
        if (aqi <= 50) return 'bg-emerald-950/30 border-emerald-900/50';
        if (aqi <= 100) return 'bg-yellow-950/30 border-yellow-900/50';
        if (aqi <= 150) return 'bg-orange-950/30 border-orange-900/50';
        if (aqi <= 200) return 'bg-red-950/30 border-red-900/50';
        if (aqi <= 300) return 'bg-purple-950/30 border-purple-900/50';
        return 'bg-rose-950/30 border-rose-900/50';
    };

    return (
        <div className="min-h-screen bg-neutral-950 text-neutral-50 p-6 md:p-10 font-sans">
            <div className="max-w-7xl mx-auto space-y-8">

                {/* Header Section */}
                <div className="flex flex-col md:flex-row items-start md:items-center justify-between gap-4 border-b border-neutral-800 pb-6">
                    <div>
                        <h1 className="text-3xl font-bold tracking-tight text-white flex items-center gap-3">
                            <Activity className="text-emerald-400" />
                            Antigravity AQI Intelligence
                        </h1>
                        <p className="text-neutral-400 mt-1">Real-time edge IoT monitoring & ML anomaly detection</p>
                    </div>

                    <div className="flex items-center gap-4">
                        <Badge variant="outline" className={`px-3 py-1 text-sm border-neutral-700 flex items-center gap-2`}>
                            <div className={`w-2 h-2 rounded-full ${isConnected ? 'bg-emerald-500 animate-pulse' : 'bg-red-500'}`} />
                            {isConnected ? 'Backend Connected' : 'Disconnected'}
                        </Badge>
                        <Button
                            onClick={handleCalibrate}
                            variant="outline"
                            className="border-neutral-700 hover:bg-neutral-800 text-neutral-200"
                        >
                            <Settings2 className="w-4 h-4 mr-2" />
                            Calibrate Sensors
                        </Button>
                    </div>
                </div>

                {/* Total AQI + Temperature & Humidity Row */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                    {/* Total AQI Card - Prominent */}
                    <Card className={`border ${getAqiBgColor(mlStatus.total_aqi)}`}>
                        <CardHeader>
                            <CardTitle className="text-neutral-200 flex items-center gap-2">
                                <Gauge className="w-5 h-5 text-neutral-400" />
                                Total AQI Index
                            </CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-center">
                                <p className={`text-6xl font-bold font-mono ${getAqiColor(mlStatus.total_aqi)}`}>
                                    {mlStatus.total_aqi}
                                </p>
                                <p className={`text-lg font-semibold mt-2 ${getAqiColor(mlStatus.total_aqi)}`}>
                                    {mlStatus.aqi_category}
                                </p>
                                <p className="text-xs text-neutral-500 mt-1">EPA Scale (0-500)</p>
                            </div>
                        </CardContent>
                    </Card>

                    {/* Temperature Card */}
                    <Card className="bg-neutral-900 border-neutral-800">
                        <CardHeader>
                            <CardTitle className="text-neutral-200 flex items-center gap-2">
                                <Thermometer className="w-5 h-5 text-red-400" />
                                Temperature
                            </CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-center p-4 bg-neutral-950 rounded-lg border border-neutral-800">
                                <p className="text-5xl font-mono text-red-400">
                                    {mlStatus.temperature}°C
                                </p>
                                <p className="text-xs text-neutral-500 mt-2">DHT11 Sensor</p>
                            </div>
                        </CardContent>
                    </Card>

                    {/* Humidity Card */}
                    <Card className="bg-neutral-900 border-neutral-800">
                        <CardHeader>
                            <CardTitle className="text-neutral-200 flex items-center gap-2">
                                <Droplets className="w-5 h-5 text-cyan-400" />
                                Humidity
                            </CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className="text-center p-4 bg-neutral-950 rounded-lg border border-neutral-800">
                                <p className="text-5xl font-mono text-cyan-400">
                                    {mlStatus.humidity}%
                                </p>
                                <p className="text-xs text-neutral-500 mt-2">DHT11 Sensor</p>
                            </div>
                        </CardContent>
                    </Card>
                </div>

                {/* Intelligence Layer Overview */}
                <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
                    <Card className="bg-neutral-900 border-neutral-800 md:col-span-2">
                        <CardHeader>
                            <CardTitle className="text-neutral-200">System Status (Scikit-Learn Inference)</CardTitle>
                        </CardHeader>
                        <CardContent>
                            <div className={`p-6 rounded-lg border ${mlStatus.anomaly ? 'bg-red-950/30 border-red-900/50' : 'bg-emerald-950/30 border-emerald-900/50'} flex items-center gap-4`}>
                                {mlStatus.anomaly ? <AlertTriangle className="w-10 h-10 text-red-500" /> : <CheckCircle className="w-10 h-10 text-emerald-500" />}
                                <div>
                                    <h3 className={`text-2xl font-semibold ${mlStatus.anomaly ? 'text-red-400' : 'text-emerald-400'}`}>
                                        {mlStatus.status}
                                    </h3>
                                    <p className="text-neutral-400 mt-1">ML Risk Score: <span className="text-white font-mono">{mlStatus.risk_score}</span></p>
                                </div>
                            </div>
                        </CardContent>
                    </Card>

                    <Card className="bg-neutral-900 border-neutral-800">
                        <CardHeader>
                            <CardTitle className="text-neutral-200 flex items-center justify-between">
                                Exhaust Fan
                                <Fan className={`w-5 h-5 text-blue-400 ${mlStatus.recommended_fan_speed > 0 ? 'animate-spin' : ''}`} />
                            </CardTitle>
                        </CardHeader>
                        <CardContent className="space-y-4">
                            <div className="text-center p-4 bg-neutral-950 rounded-lg border border-neutral-800">
                                <p className="text-sm text-neutral-400 mb-1">Current PWM Duty Cycle</p>
                                <p className="text-4xl font-mono text-blue-400">{Math.round((mlStatus.recommended_fan_speed / 255) * 100)}%</p>
                            </div>
                            <div className="flex gap-2">
                                <Button onClick={() => handleManualFan(0)} variant="destructive" className="flex-1 bg-red-900/50 hover:bg-red-900 text-red-200">OFF</Button>
                                <Button onClick={() => handleManualFan(128)} variant="secondary" className="flex-1 bg-blue-900/40 hover:bg-blue-900/60 text-blue-200">50%</Button>
                                <Button onClick={() => handleManualFan(255)} variant="default" className="flex-1 bg-blue-600 hover:bg-blue-500">MAX</Button>
                            </div>
                        </CardContent>
                    </Card>
                </div>

                {/* Telemetry Grid */}
                <h2 className="text-xl font-semibold text-neutral-200 flex items-center gap-2">
                    <Wind className="w-5 h-5 text-neutral-400" /> Live Edge Telemetry
                </h2>
                <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
                    {[
                        { label: "MQ-135 (Air Qual)", value: sensorData.mq135, color: "text-amber-400" },
                        { label: "MQ-8 (Hydrogen)", value: sensorData.mq8, color: "text-purple-400" },
                        { label: "MQ-9 (CO/Combustible)", value: sensorData.mq9, color: "text-orange-400" },
                        { label: "Dust/PM2.5", value: sensorData.dust, color: "text-slate-400" },
                    ].map((sensor, idx) => (
                        <Card key={idx} className="bg-neutral-900 border-neutral-800">
                            <CardContent className="p-6">
                                <p className="text-sm text-neutral-400">{sensor.label}</p>
                                <p className={`text-3xl font-mono mt-2 ${sensor.color}`}>{sensor.value}</p>
                                <p className="text-xs text-neutral-500 mt-1">Analog baseline corrected</p>
                            </CardContent>
                        </Card>
                    ))}
                </div>

                {/* Real-time Chart */}
                <Card className="bg-neutral-900 border-neutral-800">
                    <CardHeader>
                        <CardTitle className="text-neutral-200">AQI & Risk Score Timeline</CardTitle>
                    </CardHeader>
                    <CardContent>
                        <div className="h-72 w-full mt-4">
                            <ResponsiveContainer width="100%" height="100%">
                                <LineChart data={history}>
                                    <CartesianGrid strokeDasharray="3 3" stroke="#262626" />
                                    <XAxis dataKey="time" stroke="#525252" fontSize={12} tickMargin={10} />
                                    <YAxis stroke="#525252" fontSize={12} tickMargin={10} />
                                    <Tooltip
                                        contentStyle={{ backgroundColor: '#171717', border: '1px solid #404040', borderRadius: '8px' }}
                                        itemStyle={{ color: '#e5e5e5' }}
                                    />
                                    <Line type="monotone" dataKey="risk" name="Risk Score" stroke="#10b981" strokeWidth={2} dot={false} activeDot={{ r: 6 }} />
                                    <Line type="monotone" dataKey="aqi" name="Total AQI" stroke="#f59e0b" strokeWidth={2} dot={false} activeDot={{ r: 6 }} />
                                </LineChart>
                            </ResponsiveContainer>
                        </div>
                    </CardContent>
                </Card>

            </div>
        </div>
    );
}
