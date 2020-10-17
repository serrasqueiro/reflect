/* setup_suicide() and other functions
 */

void setup_suicide()
{
   int timeToDie;
   int mult;
   char* aString = getpass("Suicide timeout (Xs-secs, Xm-mins, Xh-hours): \n\
");

   switch(aString[strlen(aString)-1])
   {
      case 's':
         mult = 1;
         break;
      case 'm':
         mult = 60;
         break;
      case 'h':
         mult = 3600;
         break;
      default:
         mult = 0;
         break;
   }

   aString[strlen(aString)-1] = 0;
   timeToDie = atoi(aString) * mult;
   printf("Dying in %d seconds...\n", timeToDie);
   alarm(timeToDie);
}
