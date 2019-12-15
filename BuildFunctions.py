import os

Import('env')

def createExecutable(env, executableName, outputDirectory, sourceFiles):
   
    objectList = []
    
    for sourceFile in sourceFiles:
        sourceFilePath = File(sourceFile).get_abspath()       
        sourceFileName = File(sourceFile).name
    
        objectFilePath = outputDirectory + '/' + os.path.splitext(sourceFileName)[0]       
        objectFile = env.Object(outputDirectory + '/' + objectFilePath + '.o', sourceFilePath)
        
        objectList.append(objectFile)
    
    program = env.Program(outputDirectory + '/' + executableName + '.elf', objectList)   
    env.Alias(executableName, program)
     
    return program

def createLibrary(env, libraryName, outputDirectory, sourceFiles):
   
    fullLibraryName = 'lib' + libraryName.lower()
    objectList = []
    
    for sourceFile in sourceFiles:
        sourceFilePath = File(sourceFile).get_abspath()       
        sourceFileName = File(sourceFile).name
    
        objectFilePath = outputDirectory + '/' + os.path.splitext(sourceFileName)[0]       
        objectFile = env.Object(outputDirectory + '/' + objectFilePath + '.o', sourceFilePath)
        
        objectList.append(objectFile)
    
    program = env.StaticLibrary(outputDirectory + '/' + fullLibraryName + '.a', objectList)   
    env.Alias(libraryName, program)
     
    return program
         
env.AddMethod(createExecutable, 'createExecutable')
env.AddMethod(createLibrary, 'createLibrary')