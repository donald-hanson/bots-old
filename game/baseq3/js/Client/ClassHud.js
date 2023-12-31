ClassHud = {
    _Draw : function (ps, funcName)
    {
        var cls = ps.persistant[Constants.Persistant.Class];
        for (var n in Constants.Class)
        {
            if (cls == Constants.Class[n])
            {
                var other = ClassHud[n];
                if (other)
                {
                    var func = other[funcName];
                    if (func)
                        func(ps);
                }
            }
        }        
    },

    DrawTopRight: function(ps)
    {
        ClassHud._Draw(ps, "DrawTopRight");
    },

    Captain: {
        DrawTopRight: function(ps)
        {
            var cs = Sys.GetClassState();
			
			var messages = [];
			
            var bfgMode = Constants.GetName(Constants.BFGMode, cs.bfgMode);
            var bfgModeName = Constants.GetValue(Constants.BFGModeNames, bfgMode);
			
			messages.push("Mode: " + bfgModeName);
						
			if (cs.warcryActive)
				messages.push("Warcry: On  (" + cs.warcryTime.toString() + "s)");
			else
				messages.push("Warcry: Off (" + cs.warcryTime.toString() + "s)");			
					
            for(i=0;i<messages.length;i++)
                Hud.DrawSmallString(640, 32 + (i * 16), messages[i], 1.0, Constants.Hud.Alignment.Right);					
        }
    },

    Bodyguard: {
        DrawTopRight: function(ps)
        {
            var level = ps.persistant[Constants.Persistant.Level];
            var cs = Sys.GetClassState();

            var laserLevel = level < 1 ? 1 : level;
            var messages = [];
            for (i=0;i<3;i++)
            {
                if (laserLevel < i+1)
                    continue;

                var laserState = cs.lasers[i];
                var status = "";
                if (!laserState.active)
                    status = "N/A";
                else if (laserState.on)
                    status = "On ";
                else
                    status = "Off";
                messages.push("Laser " + (i + 1) + ": " + status);
            }
            
            if (level > 3)
                messages.push("Pulse: " + (cs.pulse ? "On " : "Off"));

            if (level > 0)
            {
                if (cs.decoyActive)
                    messages.push("Decoy: On  (" + cs.decoyTime.toString() + "s)");
                else
                    messages.push("Decoy: Off (" + cs.decoyTime.toString() + "s)");
            }

            if (level > 0)
                messages.push("Protect: " + (cs.protect ? "On " : "Off"));
           
            for(i=0;i<messages.length;i++)
                Hud.DrawSmallString(640, 32 + (i * 16), messages[i], 1.0, Constants.Hud.Alignment.Right);
        }
    },

    Sniper: {
        DrawTopRight: function(ps)
        {
        }
    },

    Soldier: {
        DrawTopRight: function(ps)
        {
            var level = ps.persistant[Constants.Persistant.Level];
            
            var cs = Sys.GetClassState();
            var rocketMode = Constants.GetName(Constants.RocketMode, cs.rocketMode);
            var rocketModeName = Constants.GetValue(Constants.RocketModeNames, rocketMode);
            Hud.DrawSmallString(640, 32, "Mode: " + rocketModeName, 1.0, Constants.Hud.Alignment.Right);

            if (level > 0)
            {
                Hud.DrawSmallString(640, 48, "Conquer:     ", 1.0, Constants.Hud.Alignment.Right);

                var color = Constants.Colors.White;
                var conquerLabel = "";
                if (cs.conquerActive)
                {
                    var percent = cs.distance / cs.maxDistance;
                    if (percent > 0.75)
                    {
                        var ms = Sys.Milliseconds();
                        var lowLight = Util.Darken(Constants.Colors.Red);
                        var t = 0.5 + 0.5 * Math.sin(ms / 100.0);
                        color = Util.LerpColor(Constants.Colors.Red, lowLight, t);
                    } else if (percent > 0.50)
                        color = Constants.Colors.Red;
                    else if (percent > 0.25)
                        color = Constants.Colors.Yellow;
                    else
                        color = Constants.Colors.Green;

                    conquerLabel = Math.floor(cs.distance).toString();
                } else
                {
                    conquerLabel = "Off";
                }

                Hud.DrawSmallStringColor(640, 48, conquerLabel, color, Constants.Hud.Alignment.Right);
            }
        }
    },

    Berzerker: {
        DrawTopRight: function(ps)
        {
            var cs = Sys.GetClassState();
			
			var messages = [];
									
			if (cs.chargeActive)
				messages.push("Charge: On (" + cs.chargeTime.toString() + "s)");
			else
				messages.push("Charge: Off (" + cs.chargeTime.toString() + "s)");			
				
			if (cs.rageActive)
				messages.push("Rage: On (" + cs.rageTime.toString() + "s)");
			else
				messages.push("Rage: Off (" + cs.rageTime.toString() + "s)");
				
			if (cs.autoQuadActive)
				messages.push("Auto Quad: On (" + cs.autoQuadTime.toString() + "s)");
			else
				messages.push("Auto Quad: Off");
					
            for(i=0;i<messages.length;i++)
                Hud.DrawSmallString(640, 32 + (i * 16), messages[i], 1.0, Constants.Hud.Alignment.Right);							
        }
    },

    Infiltrator: {
        DrawTopRight: function(ps)
        {
            var level = ps.persistant[Constants.Persistant.Level];
            
            var cs = Sys.GetClassState();            
            
            Hud.DrawSmallString(640, 32, "Steal: " + (cs.stealEnabled ? "Yes" : "No"), 1.0, Constants.Hud.Alignment.Right);
            
            if (level > 0)
            {
                var teamValue = cs.disguiseTeam;
                var classValue = cs.disguiseClass;

                var disguise = "None";
                if (teamValue != 0 && classValue != 0)
                {
                    var teamName = Constants.GetName(Constants.Team, teamValue);
                    var className = Constants.GetName(Constants.Class, classValue);
                    disguise = teamName + " " + className;
                }
                
                Hud.DrawSmallString(640, 48, "Disguise: " + disguise, 1.0, Constants.Hud.Alignment.Right);
            }
        }
    },

    Kamikazee: {
        DrawTopRight: function(ps)
        {
            var cs = Sys.GetClassState();
			
			var messages = [];
			
            var grenadeLauncherMode = Constants.GetName(Constants.GrenadeLauncherMode, cs.grenadeLauncherMode);
            var grenadeLauncherModeName = Constants.GetValue(Constants.GrenadeLauncherModeNames, grenadeLauncherMode);
			
			messages.push("Launcher Mode: " + grenadeLauncherModeName);
			
			var detpipeMode = Constants.GetName(Constants.DetpipeMode, cs.detpipeMode);
			var detpipeModeName = Constants.GetValue(Constants.DetpipeModeNames, detpipeMode);
			
			messages.push("DetPipe Mode: " + detpipeModeName);
			
			var t1 = cs.detpipeCount[0].toString();
			var t2 = cs.detpipeCount[1].toString();
			var l = cs.detpipeLimit.toString()
			messages.push("DetPipes: " + t1 + "+" + t2 + "/" + l);
			
            for(i=0;i<messages.length;i++)
                Hud.DrawSmallString(640, 32 + (i * 16), messages[i], 1.0, Constants.Hud.Alignment.Right);										
        }
    },
    
    Nurse: {
        DrawTopRight: function(ps)
        {
        }
    },
    
    Scientist: {
        DrawTopRight: function(ps)
        {
        }
    }
};