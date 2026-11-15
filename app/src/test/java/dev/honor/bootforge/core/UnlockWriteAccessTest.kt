package dev.honor.bootforge.core

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class UnlockWriteAccessTest {
    private fun script(name: String): String {
        val assets = generateSequence(File(System.getProperty("user.dir")).absoluteFile) { it.parentFile }
            .map { File(it, "app/src/main/assets/recovery") }
            .firstOrNull { it.isDirectory }
            ?: error("Recovery script source directory was not found")
        return File(assets, name).readText(Charsets.UTF_8)
    }

    private fun assertOrdered(script: String, vararg markers: String) {
        var previous = 0
        for (marker in markers) {
            val position = script.indexOf(marker, previous)
            assertTrue("Missing or out-of-order script marker: $marker", position >= previous)
            previous = position + marker.length
        }
    }

    @Test fun onlineUnlockSetsAllTablePartitionsRwBeforeAdjustmentAndFlashing() {
        assertOrdered(script("online-create.sh"),
            "BF_WRITE_AUTHORIZED=1\nset_unlock_targets_rw",
            "run_f2fs_shrink \"\$JOB_DIR/f2fs-shrink.log\"",
            "bf_run_gpt_apply",
            "STAGE=flash-writable-preparation\nset_unlock_targets_rw",
            "write_selected_abl_targets \"\$JOB_DIR\"",
            "write_direct \"\$DISK\" \"\$EFI_OFFSET\"")
    }

    @Test fun recoveryUnlockSetsAllTablePartitionsRwBeforeAdjustmentAndFlashing() {
        assertOrdered(script("unlock.sh"),
            "BF_WRITE_AUTHORIZED=1\nset_unlock_targets_rw",
            "run_ext4_write resize2fs",
            "bf_run_gpt_apply",
            "validate_efisp_target\nset_unlock_targets_rw",
            "write_selected_abl_targets \"\$BACKUP_DIR\"",
            "write_direct \"\$EFISP\"")
    }

    @Test fun setrwLoopCoversWholeTablesWithoutExtraTargetChecks() {
        val dispatch = script("tool-dispatch.sh")
        val function = dispatch.substringAfter("bf_setrw_partition_table() {").substringBefore("\n}\n")
        assertTrue(function.contains("bf_replayable blockdev --setrw \"\$bf_disk\""))
        assertTrue(function.contains("bf_replayable blockdev --setrw \"/dev/block/\${bf_partition##*/}\""))
        assertFalse(function.contains("bf_target_identity"))
        assertFalse(function.contains("bf_readonly_state"))
        assertFalse(function.contains("bf_ensure"))
        for (name in listOf("online-create.sh", "unlock.sh")) {
            val script = script(name)
            assertTrue(name, script.contains("bf_setrw_partition_table \"\$DISK\""))
            assertTrue(name, script.contains("bf_setrw_partition_table \"/dev/block/\$ABL_PARENT\""))
            assertTrue(name, script.contains("bf_setrw_partition_table \"/dev/block/\$OTHER_ABL_PARENT\""))
        }
        assertTrue(script("online-create.sh").contains("bf_replayable blockdev --setrw \"\$data_target\""))
    }
}
