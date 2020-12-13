package com.example.ppa;

import android.app.Activity;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothGattCharacteristic;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.graphics.Color;
import android.icu.util.Calendar;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ExpandableListView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.SeekBar;
import android.widget.SimpleExpandableListAdapter;
import android.widget.TextView;

import com.amazonaws.mobile.client.AWSMobileClient;
import com.amazonaws.regions.Region;
import com.amazonaws.services.iot.AWSIotClient;
import com.amazonaws.services.iot.model.AttachPolicyRequest;
import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.components.Legend;
import com.github.mikephil.charting.data.Entry;
import com.github.mikephil.charting.data.LineData;
import com.github.mikephil.charting.data.LineDataSet;

import com.amazonaws.mobileconnectors.iot.AWSIotMqttManager;
import com.github.mikephil.charting.listener.OnChartValueSelectedListener;
import com.github.mikephil.charting.renderer.LineChartRenderer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.UUID;


/**
 * For a given BLE device, this Activity provides the user interface to connect, display data,
 * and display GATT services and characteristics supported by the device.  The Activity
 * communicates with {@code BluetoothLeService}, which in turn interacts with the
 * Bluetooth LE API.
 */
public class DeviceControlActivity extends Activity {
    private final static String TAG = DeviceControlActivity.class.getSimpleName();

    public static final String EXTRAS_DEVICE_NAME = "DEVICE_NAME";
    public static final String EXTRAS_DEVICE_ADDRESS = "DEVICE_ADDRESS";


    private TextView mConnectionState;
    private TextView mDataField;
    private String mDeviceName;
    private String mDeviceAddress;
    private ExpandableListView mGattServicesList;
    private BluetoothLeService mBluetoothLeService;
    private ArrayList<ArrayList<BluetoothGattCharacteristic>> mGattCharacteristics =
            new ArrayList<ArrayList<BluetoothGattCharacteristic>>();
    private boolean mConnected = false;
    private BluetoothGattCharacteristic mNotifyCharacteristic;

    private final String LIST_NAME = "NAME";
    private final String LIST_UUID = "UUID";

    private TextView vdutTextView;
    private TextView vcgTextView;
    private EditText vdutEditText;
    //private EditText resEditText;
    private EditText vcgEditText;
    private Button vdutButton;
    private Button vcgButton;
    private Button startStopButton;
    private RadioGroup resRadioGroup;
    private SeekBar sampleRateSeekBar;
    private TextView sampleRateTextView;
    private String fileName;


    private ArrayList<Entry> current_values = new ArrayList<>();
    private LineChart current_LineChart;

    private float vdut_value = 0.0f;
    private float vcg_value = 0.0f;
    private int res_value = 0;

    private float current_readout = 0.0f;
    private double startTime = 0.0;
    private int sampleTimePerStep = 50;
    private int sampleRate = 0;
    private float gain = (float) 2.0;
    private float resistor_value = (float) 1000;
    private float resistor_values_array[] = {(float) 1000.0, (float) 10000.0, (float) 50000.0, (float) 100000.0};
    private float vref = (float) 5.0;
    private float maxrange = (float) 0x3FFFF;
    private byte res_value_adjusted;
    private UUID readout_char_uuid = UUID.fromString("7a28c888-02f9-11eb-adc1-0242ac120002");
    private UUID ppa_service_uuid = UUID.fromString("10cb4c5c-278f-11eb-adc1-0242ac120002");

    String UUID_res_value = "7b26228e-0ef8-11eb-adc1-0242ac120002";
    String UUID_dacexternal_value = "711d90da-0334-11eb-adc1-0242ac120002";
    String UUID_dacinternal_value = "f3cd69ee-265f-11eb-adc1-0242ac120002";
    String UUID_samplerate_value = "3ecc08da-3a09-11eb-adc1-0242ac120002";

    // AWSIotMqttManager mqttManager = new AWSIotMqttManager("870673545030", "a2rwvdv9c2khqd-ats.iot.us-east-2.amazonaws.com");
    //AttachPolicyRequest attachPolicyReq = new AttachPolicyRequest();


    // Code to manage Service lifecycle.
    private final ServiceConnection mServiceConnection = new ServiceConnection() {

        @Override
        public void onServiceConnected(ComponentName componentName, IBinder service) {
            mBluetoothLeService = ((BluetoothLeService.LocalBinder) service).getService();
            if (!mBluetoothLeService.initialize()) {
                Log.e(TAG, "Unable to initialize Bluetooth");
                finish();
            }
            // Automatically connects to the device upon successful start-up initialization.
            mBluetoothLeService.connect(mDeviceAddress);
        }

        @Override
        public void onServiceDisconnected(ComponentName componentName) {
            mBluetoothLeService = null;
        }
    };


    // Handles various events fired by the Service.
    // ACTION_GATT_CONNECTED: connected to a GATT server.
    // ACTION_GATT_DISCONNECTED: disconnected from a GATT server.
    // ACTION_GATT_SERVICES_DISCOVERED: discovered GATT services.
    // ACTION_DATA_AVAILABLE: received data from the device.  This can be a result of read
    //                        or notification operations.
    private final BroadcastReceiver mGattUpdateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final String action = intent.getAction();
            if (BluetoothLeService.ACTION_GATT_CONNECTED.equals(action)) {
                mConnected = true;
                updateConnectionState(R.string.connected);
                mConnectionState.setText("Connected");
                invalidateOptionsMenu();
            } else if (BluetoothLeService.ACTION_GATT_DISCONNECTED.equals(action)) {
                mConnected = false;
                updateConnectionState(R.string.disconnected);
                mConnectionState.setText("Disconnected");
                invalidateOptionsMenu();
                clearUI();
            } else if (BluetoothLeService.ACTION_GATT_SERVICES_DISCOVERED.equals(action)) {
                // Show all the supported services and characteristics on the user interface.
                //   displayGattServices(mBluetoothLeService.getSupportedGattServices());
            } else if (BluetoothLeService.ACTION_DATA_AVAILABLE.equals(action)) {
                // turn 18-bit received data into a long and then convert it into float by receive_data/max_data * 5.0 / (gain * resistor) * 1000000 to convert to uA
                current_readout = (float) (Long.parseLong(intent.getStringExtra(BluetoothLeService.EXTRA_DATA).replaceAll("[^0-9A-F]", ""), 16)) / maxrange * vref / (gain * resistor_value) * 1000000;
                System.out.println(current_readout);
                mDataField.setText("Current value: " + current_readout + "uA");//intent.getStringExtra(BluetoothLeService.EXTRA_DATA));


                //mDataField.setText("Current value: " + current_readout);
                //current_readout = 0;
                if(current_readout < 2000.0){
                    dataParser(current_readout);
                }
            }
        }
    };


    private void initializeChart() {
        current_LineChart = findViewById(R.id.readout_LineChart);
        current_LineChart.setDrawGridBackground(false);

        // no description text
        current_LineChart.getDescription().setEnabled(false);

        // enable touch gestures
        current_LineChart.setTouchEnabled(true);

        //current_LineChart.setBackgroundColor(R.color.colorPrimary);
        current_LineChart.getXAxis().setAxisLineColor(R.color.cityLights);
        current_LineChart.getAxisLeft().setAxisLineColor(R.color.cityLights);
        current_LineChart.getXAxis().setTextColor(R.color.cityLights);
        current_LineChart.getAxisLeft().setTextColor(R.color.cityLights);


        // enable scaling and dragging
        current_LineChart.setDragEnabled(true);
        current_LineChart.setScaleEnabled(true);
        current_LineChart.setScaleY(1.0f);

        // if disabled, scaling can be done on x- and y-axis separately
        current_LineChart.setPinchZoom(true);

        current_LineChart.getAxisLeft().setDrawGridLines(false);
        current_LineChart.getAxisRight().setEnabled(false);
        current_LineChart.getXAxis().setDrawGridLines(false);
        current_LineChart.getXAxis().setDrawAxisLine(false);

    }

    private void updateChart() {
        current_LineChart.resetTracking();

        setChartData();
        current_LineChart.invalidate();
    }

    private void setChartData() {
        LineDataSet currentValuesSet = new LineDataSet(current_values, "DUT current");

        currentValuesSet.setColor(R.color.cityLights);
        currentValuesSet.setLineWidth(1.0f);
        currentValuesSet.setDrawCircles(false);
        currentValuesSet.setDrawValues(false);
        currentValuesSet.setMode(LineDataSet.Mode.LINEAR);
        currentValuesSet.setDrawFilled(false);

        LineData pHValuesData = new LineData(currentValuesSet);

        current_LineChart.setData(pHValuesData);
        current_LineChart.notifyDataSetChanged();

        Legend legend = current_LineChart.getLegend();
        legend.setEnabled(true);
    }

    private void dataParser(float current_readout) {
        double currentTime;

        if (startTime == 0.0) {
            startTime = Calendar.getInstance().getTimeInMillis();
            currentTime = startTime;
        } else {
            currentTime = Calendar.getInstance().getTimeInMillis();
        }

        double time = (currentTime - startTime) / 1000.0;

        current_values.add(new Entry((float) time, current_readout));

        updateChart();
    }


    private void clearUI() {
        //mGattServicesList.setAdapter((SimpleExpandableListAdapter) null);
        mDataField.setText(R.string.no_data);
    }

    public void onRadioButtonClicked(View view) {
        // Is the button now checked?
        boolean checked = ((RadioButton) view).isChecked();

        // Check which radio button was clicked
        switch (view.getId()) {
            case R.id.res1_radioButton:
                if (checked)
                    res_value_adjusted = (byte) 1;
                mBluetoothLeService.writeCustomCharacteristic(res_value_adjusted, UUID_res_value);
                break;
            case R.id.res2_radioButton:
                if (checked)
                    res_value_adjusted = (byte) 2;
                mBluetoothLeService.writeCustomCharacteristic(res_value_adjusted, UUID_res_value);
                break;
            case R.id.res3_radioButton:
                if (checked)
                    res_value_adjusted = (byte) 3;
                mBluetoothLeService.writeCustomCharacteristic(res_value_adjusted, UUID_res_value);
                break;
            case R.id.res4_radioButton:
                if (checked)
                    res_value_adjusted = (byte) 4;
                mBluetoothLeService.writeCustomCharacteristic(res_value_adjusted, UUID_res_value);
                break;
        }

        resistor_value = resistor_values_array[res_value_adjusted-1];
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        /**
         attachPolicyReq.setPolicyName("myIOTPolicy");
         attachPolicyReq.setTarget(AWSMobileClient.getInstance().getIdentityId());
         AWSIotClient mIotAndroidClient = new AWSIotClient(AWSMobileClient.getInstance());
         mIotAndroidClient.setRegion(Region.getRegion("us-east-2"));
         mIotAndroidClient.attachPolicy(attachPolicyReq);
         **/
        super.onCreate(savedInstanceState);
        setContentView(R.layout.visualization_constrained);


        final Intent intent = getIntent();
        mDeviceName = intent.getStringExtra(EXTRAS_DEVICE_NAME);
        mDeviceAddress = intent.getStringExtra(EXTRAS_DEVICE_ADDRESS);

        // Sets up UI references.
        ((TextView) findViewById(R.id.device_name_TextView)).setText(mDeviceName);

        //mGattServicesList = (ExpandableListView) findViewById(R.id.gatt_services_list);
        // mGattServicesList.setOnChildClickListener(servicesListClickListner);
        mConnectionState = (TextView) findViewById(R.id.connection_state_TextView);

        mDataField = (TextView) findViewById(R.id.data_value_TextView);

        getActionBar().setTitle(mDeviceName);
        getActionBar().setDisplayHomeAsUpEnabled(true);
        Intent gattServiceIntent = new Intent(this, BluetoothLeService.class);
        bindService(gattServiceIntent, mServiceConnection, BIND_AUTO_CREATE);

        initializeChart();

        vdutTextView = findViewById(R.id.vds_TextView);
        vcgTextView = findViewById(R.id.vcg_TextView);
        vdutEditText = findViewById(R.id.vds_EditText);
        vcgEditText = findViewById(R.id.vcg_EditText);
        vdutButton = findViewById(R.id.vds_Button);
        vcgButton = findViewById(R.id.vcg_Button);
        resRadioGroup = findViewById(R.id.res_RadioGroup);
        sampleRateSeekBar = findViewById(R.id.sampleRate_SeekBar);
        sampleRateTextView = findViewById(R.id.sampleRate_TextView);

        vdutTextView.setText("DUT voltage: " + vdut_value);
        vcgTextView.setText("CG voltage: " + vcg_value);
        mDataField.setText("Current readout: unknown");

        startStopButton = findViewById(R.id.start_Button);

        //Set and transmit new VDS value
/**
 resButton.setOnClickListener(new View.OnClickListener() {
@Override public void onClick(View v) {
try {
res_value = Integer.parseInt(resEditText.getText().toString());
} catch (Exception e) {
e.printStackTrace();
}

// vdsTextView.setText("Vds value: " + vds_value);
byte res_value_adjusted = (byte) (res_value);
mBluetoothLeService.writeCustomCharacteristic(res_value_adjusted, UUID_res_value);
}
});
 **/

        sampleRateTextView.setText("Sample rate: " + sampleRateSeekBar.getProgress()*sampleTimePerStep);

        sampleRateSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                sampleRate = progress;
                sampleRateTextView.setText("Sample rate: " + sampleRate*sampleTimePerStep + "ms");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        vdutButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    vdut_value = Float.parseFloat(vdutEditText.getText().toString());
                } catch (Exception e) {
                    e.printStackTrace();
                }

                vdutTextView.setText("DUT voltage: " + vdut_value);
                byte vdut_value_adjusted = (byte) ((vdut_value / 3.3) * 0xFF);
                mBluetoothLeService.writeCustomCharacteristic(vdut_value_adjusted, UUID_dacinternal_value);
            }
        });

        vcgButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    vcg_value = Float.parseFloat(vcgEditText.getText().toString());
                } catch (Exception e) {
                    e.printStackTrace();
                }

                vcgTextView.setText("CG voltage: " + vcg_value);
                byte vcg_value_adjusted = (byte) ((vcg_value / 10.0) * 0xFF);
                mBluetoothLeService.writeCustomCharacteristic(vcg_value_adjusted, UUID_dacexternal_value);
            }
        });


        //Start streaming the readout characteristic

        startStopButton.setTag(1);
        startStopButton.setText("Start Monitoring");

        startStopButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final int status = (Integer) v.getTag();



                if (status == 1) {
                    byte sampleRate_adjusted = (byte) sampleRateSeekBar.getProgress();
                    mBluetoothLeService.writeCustomCharacteristic(sampleRate_adjusted, UUID_samplerate_value);

                    final Handler handler = new Handler();
                    handler.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            // Do something after 5s = 5000ms
                            mBluetoothLeService.enableCharacteristicNotification(ppa_service_uuid, readout_char_uuid);
                        }
                    }, 1000);

                    fileName = "measurement_" + Calendar.getInstance().getTime().toString();
                    System.out.println(fileName);
                    startStopButton.setText("Stop monitoring");
                    startStopButton.setTag(0);

                    sampleRateSeekBar.setEnabled(false);
                    vdutButton.setEnabled(false);
                    vcgButton.setEnabled(false);
                    vdutEditText.setEnabled(false);
                    vcgEditText.setEnabled(false);

                    for(int i = 0; i < resRadioGroup.getChildCount(); i++){
                        ((RadioButton) resRadioGroup.getChildAt(i)).setEnabled(false);
                    }

                } else {
                    try {
                        File dir = new File( DeviceControlActivity.this.getExternalFilesDir(null) + "/PPA_measurements");
                        if (!dir.exists()) {
                            dir.mkdirs();
                        }
                        File outputFile = new File(DeviceControlActivity.this.getExternalFilesDir(null) + "/PPA_measurements/" + fileName + ".txt");
                        System.out.println(DeviceControlActivity.this.getExternalFilesDir(null));
                        outputFile.createNewFile();
                        FileOutputStream outputStream = new FileOutputStream(outputFile, true);
                        OutputStreamWriter outputStreamWriter = new OutputStreamWriter(outputStream);
                        outputStreamWriter.write("Start measurement\n");

                        for (Entry s : current_values) {
                            outputStreamWriter.write(s.toString()+"\n");
                        }
                        outputStreamWriter.write("End measurement\n");
                        outputStreamWriter.close();
                        //File imageFile = new File(DeviceControlActivity.this.getExternalFilesDir(null) + "/PPA_measurements/" + fileName + ".png");
                        current_LineChart.saveToPath(fileName, DeviceControlActivity.this.getExternalFilesDir(null) + "/PPA_measurements/");
                    } catch (Exception e) {
                        e.printStackTrace();
                    }

                    mBluetoothLeService.disableCharacteristicNotification(ppa_service_uuid, readout_char_uuid);
                    startStopButton.setText("Start monitoring");
                    startStopButton.setTag(1);

                    sampleRateSeekBar.setEnabled(true);
                    vdutButton.setEnabled(true);
                    vcgButton.setEnabled(true);
                    vdutEditText.setEnabled(true);
                    vcgEditText.setEnabled(true);
                    for(int i = 0; i < resRadioGroup.getChildCount(); i++){
                        ((RadioButton) resRadioGroup.getChildAt(i)).setEnabled(true);
                    }
                }
            }
        });

    }

    @Override
    protected void onResume() {
        super.onResume();
        registerReceiver(mGattUpdateReceiver, makeGattUpdateIntentFilter());
        if (mBluetoothLeService != null) {
            final boolean result = mBluetoothLeService.connect(mDeviceAddress);
            Log.d(TAG, "Connect request result=" + result);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        unregisterReceiver(mGattUpdateReceiver);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unbindService(mServiceConnection);
        mBluetoothLeService = null;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.gatt_services, menu);
        if (mConnected) {
            menu.findItem(R.id.menu_connect).setVisible(false);
            menu.findItem(R.id.menu_disconnect).setVisible(true);
        } else {
            menu.findItem(R.id.menu_connect).setVisible(true);
            menu.findItem(R.id.menu_disconnect).setVisible(false);
        }
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.menu_connect:
                mBluetoothLeService.connect(mDeviceAddress);
                return true;
            case R.id.menu_disconnect:
                mBluetoothLeService.disconnect();
                return true;
            case android.R.id.home:
                onBackPressed();
                return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void updateConnectionState(final int resourceId) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                //  mConnectionState.setText(resourceId);
            }
        });
    }

    private static IntentFilter makeGattUpdateIntentFilter() {
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(BluetoothLeService.ACTION_GATT_CONNECTED);
        intentFilter.addAction(BluetoothLeService.ACTION_GATT_DISCONNECTED);
        intentFilter.addAction(BluetoothLeService.ACTION_GATT_SERVICES_DISCOVERED);
        intentFilter.addAction(BluetoothLeService.ACTION_DATA_AVAILABLE);
        return intentFilter;
    }

}