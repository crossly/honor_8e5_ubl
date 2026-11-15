package dev.honor.bootforge

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class RootdWriteAccessTest {
    private fun source(path: String): String {
        val root = generateSequence(File(System.getProperty("user.dir")).absoluteFile) { it.parentFile }
            .firstOrNull { File(it, path).isFile }
            ?: error("Source file was not found: $path")
        return File(root, path).readText(Charsets.UTF_8)
    }

    @Test fun rootdMaintainsPermissiveAndChecksTheInitContext() {
        val source = source("adaptation/aph136g142/src/rootd.c")
        assertTrue(source.contains("static const char ctx[] = \"u:r:init:s0\""))
        assertTrue(source.contains("maintain_permissive() < 0 || set_su_context() < 0"))
        assertTrue(source.contains("usleep(200000)"))
        val daemon = source.substringAfter("int pc98260_run_rootd(void)")
        assertTrue(daemon.indexOf("pid_t watcher = fork()") < daemon.indexOf("accept4("))
        assertTrue(source.contains("ROOT98260_CHILD_POLICY_FAILED"))
    }

    @Test fun blockProbeChecksCapabilityAndNeverWritesPartitionData() {
        val source = source("app/src/main/cpp/block_write_probe.c")
        assertTrue(source.contains("SYS_capget"))
        assertTrue(source.contains("CAP_SYS_ADMIN"))
        assertTrue(source.contains("O_RDWR | O_CLOEXEC"))
        assertTrue(source.contains("WRITE_OPEN_OK"))
        assertFalse(source.contains("write("))
    }
}
