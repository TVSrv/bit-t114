package ru.bitpet.app

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import ru.bitpet.app.ui.BitTheme
import ru.bitpet.app.ui.HomeScreen

class MainActivity : ComponentActivity() {
    private val askPermissions = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        if (PetHub.hasPermissions()) PetHub.autoConnect()
        if (PetHub.stepsOn.value) PetHub.startSteps()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent { BitTheme { HomeScreen(onNeedPermissions = { askPermissions.launch(PetHub.requiredPermissions()) }) } }
        if (!PetHub.hasPermissions()) askPermissions.launch(PetHub.requiredPermissions() + listOfNotNull(PetHub.stepsPermission()))
    }

    override fun onResume() {
        super.onResume()
        PetHub.uiVisible = true
        if (PetHub.hasPermissions()) PetHub.autoConnect()
        Sounds.stopAlarm()  // открыли приложение — будильник услышан
        PetHub.cancelAlarmNote()
    }

    override fun onPause() {
        super.onPause()
        PetHub.uiVisible = false
    }
}
