function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    component.addOperation("CreateShortcut", "@TargetDir@/KidsMode.exe",
        "@StartMenuDir@/Kids Mode.lnk",
        "workingDirectory=@TargetDir@");
    // The documented workflow: Kids Mode starts when the child logs in.
    component.addOperation("CreateShortcut", "@TargetDir@/KidsMode.exe",
        "@HomeDir@\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\Kids Mode.lnk",
        "workingDirectory=@TargetDir@");
}
