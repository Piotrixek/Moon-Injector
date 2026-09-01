import { execFile } from "child_process"
import { promisify } from "util"
import path from "path"
import fs from "fs"
import { fileURLToPath } from "url"

const executeFilePromise = promisify(execFile)
const currentDirectoryPath = path.dirname(fileURLToPath(import.meta.url))

function resolveMoonExecutablePath() {
  const primaryReleasePath = path.resolve(currentDirectoryPath, "..", "..", "build", "Release", "MoonCLI.exe")
  if (fs.existsSync(primaryReleasePath)) {
    return primaryReleasePath
  }
  const rootPath = path.resolve(currentDirectoryPath, "..", "..", "MoonCLI.exe")
  if (fs.existsSync(rootPath)) {
    return rootPath
  }
  return primaryReleasePath
}

export async function runMoonCommand(argumentList) {
  const executablePath = resolveMoonExecutablePath()
  if (!fs.existsSync(executablePath)) {
    throw new Error(`MoonCLI.exe not found at path: ${executablePath}. Please build MoonCLI first.`)
  }

  const { stdout, stderr } = await executeFilePromise(executablePath, argumentList, {
    maxBuffer: 1024 * 1024 * 20
  })

  const trimmedOutput = (stdout || "").trim()
  if (!trimmedOutput) {
    if (stderr) {
      throw new Error(stderr)
    }
    return null
  }

  try {
    return JSON.parse(trimmedOutput)
  } catch (jsonParsingError) {
    return {
      rawOutput: trimmedOutput,
      error: jsonParsingError.message
    }
  }
}
